package main

import (
	"fmt"
	"net"

	"github.com/OnePlay-Internet/webrtc-proxy/util/tool"
	"github.com/OnePlay-Internet/webrtc-proxy/util/config"
	appsink "github.com/Oneplay-Internet/screencoder/sink/appsink/go"
	"github.com/pion/webrtc/v3"
)

func main() {

	qr := tool.GetDevice()
	if len(qr.Monitors) == 0{
		fmt.Printf("no display available");
		return;
	}

	lis := &config.ListenerConfig{
		VideoSource: qr.Monitors[0],

		DataType:  "rtp",
		MediaType: "video",
		Name:      "Screencapture",
		Codec:     webrtc.MimeTypeH264,
	}

	sink,err := appsink.NewAppsink(lis);
	if err != nil{
		fmt.Printf("%s\n",err.Error());	
	}

	sink.Open();

	udp,_ := net.Dial("udp","localhost:6000")
	for {
		rtp := sink.ReadRTP()
		if rtp != nil {
			buf,_ := rtp.Marshal()
			udp.Write(buf);
		}
	}
}