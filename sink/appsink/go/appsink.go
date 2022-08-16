package appsink
import (
	"fmt"
	"time"
	"unsafe"

	"github.com/pion/rtp"
	"github.com/Oneplay-Internet/screencoder/sink/appsink/go/h264"
)


// #cgo LDFLAGS:  ${SRCDIR}/../../../build/libscreencoderlib.a ${SRCDIR}/../../../build/pre-compiled/windows/lib/libavcodec.a ${SRCDIR}/../../../build/pre-compiled/windows/lib/libavutil.a 
// #include "go_adapter.h"
import "C"

type Appsink struct {
	packetizer Packetizer
	channel chan *rtp.Packet
}


func newAppsink() *Appsink {
	app := &Appsink{};

	app.packetizer = h264.NewH264Payloader();
	app.channel = make(chan *rtp.Packet,1000);

	go func ()  {
		for {
			var size,duration  C.int;
			var data,buf  unsafe.Pointer;

			res := C.GoHandleAVPacket(&data,&buf,&size,&duration);
			if int(res) == -1 {
				fmt.Printf("Fail to get packet")
			}

			app.WriteSample(data,size,duration);
			C.GoDestroyAVPacket(buf);
		}	
	}();
	return app;
}



func (s *Appsink) WriteSample(buffer unsafe.Pointer, 
							  bufferLen C.int, 
							  C_duration C.int) {
	c_byte := C.GoBytes(buffer, bufferLen)
	duration := time.Duration(C_duration);

	// TODO clock rate
	samples := uint32(duration.Seconds() * float64(90000))
	packets := s.packetizer.Packetize(c_byte, samples)

	for _, p := range packets {
		s.channel <- p;
	}
}


func (sink *Appsink) ReadRTP() *rtp.Packet {
	return <-sink.channel;
}
