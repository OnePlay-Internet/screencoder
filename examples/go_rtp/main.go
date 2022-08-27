package main

import (
	"net"

	appsink "github.com/Oneplay-Internet/screencoder/sink/appsink/go"
)

func main() {
	udp,_ := net.Dial("udp","localhost:6000")
	sink := appsink.NewAppsink();
	for {
		rtp := sink.ReadRTP()
		if rtp != nil {
			buf,_ := rtp.Marshal()
			// fmt.Printf("receive rtp packet, %s\n",rtp.String());
			udp.Write(buf);
		}
	}
}