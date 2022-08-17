package main

import (
	"fmt"

	appsink "github.com/Oneplay-Internet/screencoder/sink/appsink/go"
)

func main() {
	sink := appsink.NewAppsink();
	for {
		rtp := sink.ReadRTP()
		if rtp != nil {
			fmt.Printf("receive rtp packet\n");
		}
	}
}