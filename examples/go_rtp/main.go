package main

import (
	"fmt"
	"net"

	"github.com/OnePlay-Internet/webrtc-proxy/util/config"
	appsink "github.com/Oneplay-Internet/screencoder/sink/appsink/go"
)

func main() {
	udp,_ := net.Dial("udp","localhost:6000")
	sink,err := appsink.NewAppsink(&config.ListenerConfig{});
	if err != nil{
		fmt.Printf("%s\n",err.Error());	
	}

	sink.Open();
	for {
		rtp := sink.ReadRTP()
		if rtp != nil {
			buf,_ := rtp.Marshal()
			fmt.Printf("receive rtp packet, %s\n",rtp.String());
			udp.Write(buf);
		}
	}
}