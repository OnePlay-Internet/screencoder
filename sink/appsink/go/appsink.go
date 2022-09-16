package appsink

import (
	"fmt"
	"unsafe"

	"github.com/OnePlay-Internet/webrtc-proxy/util/config"
	"github.com/Oneplay-Internet/screencoder/sink/appsink/go/h264"
	"github.com/pion/rtp"
	"github.com/pion/webrtc/v3/pkg/media"
)

// #cgo LDFLAGS: ${SRCDIR}/../../../buildmingw/libscreencoderlib.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libavformat.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libavcodec.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libavdevice.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libavfilter.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libavutil.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libpostproc.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libswresample.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libswscale.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libx264.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libx265.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/mingw/windows/lib/libhdr10plus.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/msys2/libz.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/msys2/liblzma.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/msys2/libbcrypt.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/msys2/libiconv.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/msys2/libstdc++.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/msys2/libwinpthread.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/msys2/libssp.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build-tools/msys2/libbz2.a
// #cgo LDFLAGS: -lksuser -lwsock32 -lws2_32 -ld3d11 -ldxgi -lD3DCompiler -lsetupapi -ldwmapi -lSecur32 -lBcrypt -lole32
// #include "go_adapter.h"
import "C"

type Appsink struct {
	packetizer Packetizer
	conf       *config.ListenerConfig

	sink     unsafe.Pointer
	shutdown unsafe.Pointer
	
	encoder  string
	display  string

	channel chan *rtp.Packet
	pktchannel chan *GoPacket
}

func NewAppsink(conf *config.ListenerConfig) (*Appsink, error) {
	app := &Appsink{}
	app.conf = conf
	app.packetizer = h264.NewH264Payloader()
	app.channel = make(chan *rtp.Packet)
	app.pktchannel = make(chan *GoPacket)

	app.display = conf.VideoSource.DeviceName


	enginesupport := []string{"nvenc_h264","nvenc_hevc","amf_h264","amf_h265"};
	clientsupport := []string{"nvenc_h264","amf_h264"};

	for _,codec := range enginesupport{
		for _,client := range clientsupport{
			if codec == client {
				available := C.QueryEncoder(C.CString(codec));
				if available == 1{
					app.encoder = codec
				}
			}
		}
	}

	fmt.Printf("Starting screencoder engine with display : %s on adapter %s\n", app.display,conf.VideoSource.Adapter);
	return app, nil
}

func (s *Appsink) writeSample(buffer unsafe.Pointer,
							bufferLen C.int,
							C_duration C.int) {

	c_byte := C.GoBytes(buffer, bufferLen)
	samples := uint32((float64(C_duration) / 1000000000) * 90000)
	packets := s.packetizer.Packetize(c_byte, samples)
	for _, p := range packets {
		s.channel <- p
	}
}

type  GoPacket struct {
	size, duration C.int
	data, buf unsafe.Pointer
}

func (app *Appsink) Open() *config.ListenerConfig {
	app.shutdown = C.NewEvent()
	if app.shutdown == nil {
		return nil
	}

	app.sink = C.AllocateAppSink();
	if app.sink == nil {
		return nil
	}

			
	go func ()  {
		C.StartScreencodeThread(app.sink, app.shutdown,
			C.CString(app.encoder),
			C.CString(app.display));
	
		fmt.Printf("screencode thread terminated\n");
	}()
			
		
		
		

	go func() {
		for {
			var pkt GoPacket;
			res := C.GoHandleAVPacket(app.sink, &pkt.data, &pkt.buf, &pkt.size, &pkt.duration)
			if res == 0 { fmt.Printf("appsink is null pointer"); return; }
			app.pktchannel <- &pkt;
		}
	}()
	go func() {
		for {
			pkt :=<-app.pktchannel;
			app.writeSample(pkt.data, pkt.size, pkt.duration)
			C.GoUnrefAVPacket(app.sink,pkt.buf)
		}
	}()
	return app.conf
}

func (sink *Appsink) ReadRTP() *rtp.Packet {
	return <-sink.channel
}

func (lis *Appsink) ReadSample() *media.Sample {
	chn := make(chan *media.Sample)
	return <-chn
}

func (lis *Appsink) Close() {
	C.RaiseEvent(lis.shutdown)
	C.StopAppSink(lis.sink);
	lis.sink = nil;
	lis.shutdown = nil;
}

