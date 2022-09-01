package appsink

import (
	"fmt"
	"strings"
	"unsafe"

	"github.com/OnePlay-Internet/webrtc-proxy/util/config"
	"github.com/Oneplay-Internet/screencoder/sink/appsink/go/h264"
	"github.com/pion/rtp"
	"github.com/pion/webrtc/v3/pkg/media"
)

// #cgo LDFLAGS: ${SRCDIR}/../../../build/libscreencoderlib.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libavformat.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libavcodec.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libavdevice.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libavfilter.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libavutil.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libpostproc.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libswresample.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libswscale.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libx264.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libx265.a
// #cgo LDFLAGS: ${SRCDIR}/../../../build/pre-compiled/windows/lib/libhdr10plus.a
// #cgo LDFLAGS: -lz -llzma -lbcrypt
// #cgo LDFLAGS: -lksuser -lwsock32 -lws2_32 -ld3d11 -ldxgi -lD3DCompiler -lsetupapi -ldwmapi -lbz2 -lSecur32 -lBcrypt -lole32
// #include "go_adapter.h"
import "C"

type Appsink struct {
	packetizer Packetizer
	conf       *config.ListenerConfig

	sink     unsafe.Pointer
	shutdown unsafe.Pointer
	encoder  string

	display  string
	displays []string

	channel chan *rtp.Packet
}

func NewAppsink(conf *config.ListenerConfig) (*Appsink, error) {
	app := &Appsink{}
	app.conf = conf
	app.packetizer = h264.NewH264Payloader()
	app.channel = make(chan *rtp.Packet, 1000)
	app.displays = make([]string, 0);
	app.encoder = "nvenc_h264"
	app.display = "";

	i := 0
	for {
		c_str := C.QueryDisplay(C.int(i))
		display := string(C.GoBytes(unsafe.Pointer(c_str), C.StringLength(c_str)))
		if display == "out of range" {
			break;
		}
		app.displays = append(app.displays, display);
		i++
	}

	for _,disp := range app.displays {
		if strings.Contains(disp,conf.Source) {
			app.display = disp;	
		}
	}

	if app.display == "" {
		return nil,fmt.Errorf("no display found");	
	}

	fmt.Printf("display name: %s\n", app.display)
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

func (app *Appsink) Open() *config.ListenerConfig {
	app.shutdown = C.NewEvent()
	if app.shutdown == nil {
		return nil
	}
	app.sink = C.AllocateAppSink()
	if app.sink == nil {
		return nil
	}

	go func ()  {
		C.StartScreencodeThread(app.sink, app.shutdown,
			C.CString(app.encoder),
			C.CString(app.display))
	
		fmt.Printf("screencode thread terminated\n");
	}()

	go func() {
		for {
			var size, duration C.int
			var data, buf unsafe.Pointer
			res := C.GoHandleAVPacket(app.sink, &data, &buf, &size, &duration)
			if res == 0 { continue; }
			app.writeSample(data, size, duration)
			C.GoUnrefAVPacket(buf)
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
}
