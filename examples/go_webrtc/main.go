package main

import (
	"fmt"
	"os"
	"time"

	proxy "github.com/OnePlay-Internet/webrtc-proxy"
	"github.com/OnePlay-Internet/webrtc-proxy/listener"
	"github.com/OnePlay-Internet/webrtc-proxy/util/config"
	"github.com/pion/webrtc/v3"
)

func main() {
	var token string
	args := os.Args[1:]
	for i, arg := range args {
		if arg == "--token" {
			token = args[i+1]
		} else if arg == "--help" {
			fmt.Printf("--token |  server token\n")
			return
		}
	}

	if token == "" {
		return
	}

	grpc := config.GrpcConfig{
		Port:          30000,
		ServerAddress: "54.169.49.176",
		Token:         token,
	}
	rtc := config.WebRTCConfig{
		Ices: []webrtc.ICEServer{{
			URLs: []string{
				"stun:stun.l.google.com:19302",
			},
		}, {
			URLs: []string{
				"stun:workstation.thinkmay.net:3478",
			},
		},
		},
	}

	br  := []*config.BroadcasterConfig{}
	lis := []*config.ListenerConfig{{
		Source: "screencoder",

		DataType: "rtp",

		MediaType: "video",
		Name:      "gpuGstreamer",
		Codec:     webrtc.MimeTypeH264,
	}}		
	
	Lists := make([]listener.Listener, 0)
	for _, lis_conf := range lis {
		var Lis listener.Listener
		if lis_conf.Source == "screencoder" {
			Lis = NewScreencoderListener(*lis_conf)
		} else {
			fmt.Printf("Unimplemented listener\n")
			continue
		}
		

		Lists = append(Lists, Lis)
	}

	for {
		chans := config.DataChannelConfig{
			Offer: true,
			Confs: map[string]*struct {
				Send    chan string
				Recv    chan string
				Channel *webrtc.DataChannel
			}{
				"hid": {
					Send:    make(chan string),
					Recv:    make(chan string),
					Channel: nil,
				},
			},
		}

		go func() {
			for {
				channel := chans.Confs["hid"]
				if channel != nil {
					str := <-chans.Confs["hid"].Recv
					go ParseHIDInput(str)
				} else {
					return
				}
			}
		}()



		prox, err := proxy.InitWebRTCProxy(nil, &grpc, &rtc, br, &chans, Lists)
		if err != nil {
			fmt.Printf("failed to init webrtc proxy, try again in 2 second\n")
			time.Sleep(2 * time.Second)
			continue
		}
		<-prox.Shutdown
	}
}
