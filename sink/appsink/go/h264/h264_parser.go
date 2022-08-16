package h264

import (
	"encoding/binary"
	"time"

	"github.com/Oneplay-Internet/screencoder/sink/appsink/go/pio"
	"github.com/pion/rtp"
)

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}



// https://stackoverflow.com/questions/24884827/possible-locations-for-sequence-picture-parameter-sets-for-h-264-stream/24890903#24890903
const (
	stapaNALUType  = 24		  // 00011000
	fubNALUType    = 29		  // 00011101
	spsNALUType    = 7		  // 00000111
	ppsNALUType    = 8		  // 00001000
	audNALUType    = 9		  // 00001001
	fillerNALUType = 12		  // 00001100
	fuaNALUType    = 28		  // 00011100

	fuaHeaderSize       = 2
	stapaHeaderSize     = 1
	stapaNALULengthSize = 2

	naluTypeBitmask   = 0x1F  // 00011111
	naluRefIdcBitmask = 0x60  // 01100000
	fuStartBitmask    = 0x80  // 10000000
	fuEndBitmask      = 0x40  // 01000000
	outputStapAHeader = 0x78  // 01111000
)

const (
	NALU_RAW = iota
	NALU_AVCC
	NALU_ANNEXB
)


// H264Payloader payloads H264 packets
type H264Payloader struct {
	spsNalu, ppsNalu []byte


	MTU              uint16
	PayloadType      uint8
	SSRC             uint32
	Sequencer        rtp.Sequencer
	Timestamp        uint32
	ClockRate        uint32
	extensionNumbers struct { // put extension numbers in here. If they're 0, the extension is disabled (0 is not a legal extension number)
		AbsSendTime int // http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
	}
	timegen func() time.Time
}

func NewH264Payloader() *H264Payloader {
	return &H264Payloader{
		Sequencer: rtp.NewRandomSequencer(),
		extensionNumbers: struct{AbsSendTime int}{AbsSendTime: 22},
		timegen: time.Now,
	}
}







func SplitNALUs(payload []byte) (nalus [][]byte, typ int) {
	if len(payload) < 4 {
		return [][]byte{payload}, NALU_RAW
	}

	val3 := pio.U24BE(payload)
	val4 := pio.U32BE(payload)

	// maybe AVCC
	if val4 <= uint32(len(payload)) {
		val4tmp := val4
		payloadtmp := payload[4:]
		nalus := [][]byte{}
		for {
			if val4tmp > uint32(len(payloadtmp)) {
				break
			}
			nalus = append(nalus, payloadtmp[:val4tmp])
			payloadtmp = payloadtmp[val4tmp:]
			if len(payloadtmp) < 4 {
				break
			}
			val4tmp = pio.U32BE(payloadtmp)
			payloadtmp = payloadtmp[4:]
			if val4tmp > uint32(len(payloadtmp)) {
				break
			}
		}
		if len(payloadtmp) == 0 {
			return nalus, NALU_AVCC
		}
	}

	// is Annex B
	if val3 == 1 || val4 == 1 {
		val3tmp := val3
		val4tmp := val4
		start := 0
		pos := 0
		for {
			if start != pos {
				nalus = append(nalus, payload[start:pos])
			}
			if val3tmp == 1 {
				pos += 3
			} else if val4tmp == 1 {
				pos += 4
			}
			start = pos
			if start == len(payload) {
				break
			}
			val3tmp = 0
			val4tmp = 0
			for pos < len(payload) {
				if pos+2 < len(payload) && payload[pos] == 0 {
					val3tmp = pio.U24BE(payload[pos:])
					if val3tmp == 0 {
						if pos+3 < len(payload) {
							val4tmp = uint32(payload[pos+3])
							if val4tmp == 1 {
								break
							}
						}
					} else if val3tmp == 1 {
						break
					}
					pos++
				} else {
					pos++
				}
			}
		}
		typ = NALU_ANNEXB
		return
	}

	return [][]byte{payload}, NALU_RAW
}


func emitNalus(nals []byte, emit func([]byte)) {

	// TODO : update libav indicator
	// findInd : find indicator
	// +----------------------------------------+
	// |0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|...
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
	// |x|x|0|0|0|0|1|x|x|x|x|x|x|x|x|x|x|x|x|x|...
	// |<--Start   					   		   |
	// |   |indLen   |     			nalu	   |
	// |   |<--indStart   					   |
	// +---------------------------------------+
	findInd := func(nalu []byte, start int) (indStart int, indLen int) {
		zCount := 0 // zeroCount

		for i, b := range nalu[start:] {
			if b == 0 {
				zCount++
				continue
			} else if b == 1 {
				if zCount >= 2 {
					return start + i - zCount, zCount + 1
				}
			}
			zCount = 0
		}
		return -1, -1 // return -1 if no indicator found
	}


	nextIndStart, nextIndLen := findInd(nals, 0)
	if nextIndStart == -1 {
		// emit on whole nals if indicator not found
		emit(nals)
	} else {
		for nextIndStart != -1 {
			prevStart := nextIndStart + nextIndLen
			nextIndStart, nextIndLen = findInd(nals, prevStart)
			if nextIndStart != -1 {
				// emit on nalu
				emit(nals[prevStart:nextIndStart])
			} else {
				// Emit until end of stream, no end indicator found
				emit(nals[prevStart:])
			}
		}
	}
}

// Payload fragments a H264 packet across one or more byte arrays
func (p *H264Payloader) Payload(mtu uint16, payload []byte) [][]byte {
	var payloads [][]byte
	if len(payload) == 0 {
		return payloads
	}
	


	emitNalus(payload, func(nalu []byte) {
		if len(nalu) == 0 {
			return
		}

		naluType   := nalu[0] & naluTypeBitmask     // AND operator on first byte of nal Unit
		naluRefIdc := nalu[0] & naluRefIdcBitmask   // AND operator on first byte of nal Unit

		switch {
		case naluType == audNALUType || naluType == fillerNALUType:
			return
		case naluType == spsNALUType:
			p.spsNalu = nalu
			return
		case naluType == ppsNALUType:
			p.ppsNalu = nalu
			return
		case p.spsNalu != nil && p.ppsNalu != nil:
			// Pack current NALU with SPS and PPS as STAP-A
			spsLen := make([]byte, 2)
			binary.BigEndian.PutUint16(spsLen, uint16(len(p.spsNalu)))

			ppsLen := make([]byte, 2)
			binary.BigEndian.PutUint16(ppsLen, uint16(len(p.ppsNalu)))

			stapANalu := []byte{outputStapAHeader}
			stapANalu = append(stapANalu, spsLen...)
			stapANalu = append(stapANalu, p.spsNalu...)
			stapANalu = append(stapANalu, ppsLen...)
			stapANalu = append(stapANalu, p.ppsNalu...)
			if len(stapANalu) <= int(mtu) {
				out := make([]byte, len(stapANalu))
				copy(out, stapANalu)
				payloads = append(payloads, out)
			}

			p.spsNalu = nil
			p.ppsNalu = nil
		}

		// Single NALU
		if len(nalu) <= int(mtu) {
			out := make([]byte, len(nalu))
			copy(out, nalu)
			payloads = append(payloads, out)
			return
		}

		// FU-A
		maxFragmentSize := int(mtu) - fuaHeaderSize

		// The FU payload consists of fragments of the payload of the fragmented
		// NAL unit so that if the fragmentation unit payloads of consecutive
		// FUs are sequentially concatenated, the payload of the fragmented NAL
		// unit can be reconstructed.  The NAL unit type octet of the fragmented
		// NAL unit is not included as such in the fragmentation unit payload,
		// but rather the information of the NAL unit type octet of the
		// fragmented NAL unit is conveyed in the F and NRI fields of the FU
		// indicator octet of the fragmentation unit and in the type field of
		// the FU header.  An FU payload MAY have any number of octets and MAY
		// be empty.

		naluData := nalu
		// According to the RFC, the first octet is skipped due to redundant information
		naluDataIndex := 1
		naluDataLength := len(nalu) - naluDataIndex
		naluDataRemaining := naluDataLength

		if min(maxFragmentSize, naluDataRemaining) <= 0 {
			return
		}

		for naluDataRemaining > 0 {
			currentFragmentSize := min(maxFragmentSize, naluDataRemaining)
			out := make([]byte, fuaHeaderSize+currentFragmentSize)

			// +---------------+
			// |0|1|2|3|4|5|6|7|
			// +-+-+-+-+-+-+-+-+
			// |F|NRI|  Type   |
			// +---------------+
			out[0] = fuaNALUType
			out[0] |= naluRefIdc

			// +---------------+
			// |0|1|2|3|4|5|6|7|
			// +-+-+-+-+-+-+-+-+
			// |S|E|R|  Type   |
			// +---------------+

			out[1] = naluType
			if naluDataRemaining == naluDataLength {
				// Set start bit
				out[1] |= 1 << 7
			} else if naluDataRemaining-currentFragmentSize == 0 {
				// Set end bit
				out[1] |= 1 << 6
			}

			copy(out[fuaHeaderSize:], naluData[naluDataIndex:naluDataIndex+currentFragmentSize])
			payloads = append(payloads, out)

			naluDataRemaining -= currentFragmentSize
			naluDataIndex += currentFragmentSize
		}
	})

	return payloads
}





// Packetize packetizes the payload of an RTP packet and returns one or more RTP packets
func (p *H264Payloader) Packetize(payload []byte, samples uint32) []*rtp.Packet {
	// Guard against an empty payload
	if len(payload) == 0 {
		return nil
	}



	payloads := p.Payload(p.MTU-12, payload)
	packets := make([]*rtp.Packet, len(payloads))

	for i, pp := range payloads {
		packets[i] = &rtp.Packet{
			Header: rtp.Header{
				Version:        2,
				Padding:        false,
				Extension:      false,
				Marker:         i == len(payloads)-1,
				PayloadType:    p.PayloadType,
				SequenceNumber: p.Sequencer.NextSequenceNumber(),
				Timestamp:      p.Timestamp, // Figure out how to do timestamps
				SSRC:           p.SSRC,
			},
			Payload: pp,
		}
	}
	p.Timestamp += samples

	if len(packets) != 0 && p.extensionNumbers.AbsSendTime != 0 {
		sendTime := rtp.NewAbsSendTimeExtension(p.timegen())
		// apply http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
		b, err := sendTime.Marshal()
		if err != nil {
			return nil // never happens
		}
		err = packets[len(packets)-1].SetExtension(uint8(p.extensionNumbers.AbsSendTime), b)
		if err != nil {
			return nil // never happens
		}
	}

	return packets
}

