package appsink

/*
#include "go_adapter.h"

*/
import "C"
import (
	"fmt"
	"time"
	"unsafe"

	"github.com/pion/rtp"
)

type Appsink struct {
	packetizer Packetizer
	channel chan *rtp.Packet
}


func newAppsink() *Appsink {
	app := &Appsink{};

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
