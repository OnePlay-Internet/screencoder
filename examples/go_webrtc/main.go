package main

import (
	"fmt"
	"os"

	proxy "github.com/OnePlay-Internet/webrtc-proxy"
	"github.com/OnePlay-Internet/webrtc-proxy/hid"
	"github.com/OnePlay-Internet/webrtc-proxy/listener"
	appsink "github.com/Oneplay-Internet/screencoder/sink/appsink/go"

	"github.com/OnePlay-Internet/webrtc-proxy/util/config"
	"github.com/pion/webrtc/v3"
)

func main() {
	var token string
	args := os.Args[1:]
	URL := "localhost:5000"

	for i, arg := range args {
		if arg == "--token" {
			token = args[i+1]
		} else if arg == "--hid" {
			URL = args[i+1]
		} else if arg == "--help" {
			fmt.Printf("--token |  server token\n")
			fmt.Printf("--hid   |  HID server URL (example: localhost:5000)\n")
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
		Source: "Display",

		DataType:  "rtp",
		MediaType: "video",
		Name:      "Screencoder",
		Codec:     webrtc.MimeTypeH264,
	},
	{
		Source: "Soundcard",

		DataType:  "sample",
		MediaType: "_audio",
		Name:      "Audicoder",
		Codec:     webrtc.MimeTypeOpus,
	}}
	
	Lists := make([]listener.Listener, 0)
	for _, lis_conf := range lis {
		var err error;
		var Lis listener.Listener
		if lis_conf.MediaType == "video" {
			Lis,err =  appsink.NewAppsink(lis_conf)
		// } else if lis_conf.MediaType == "audio" {
		// 	Lis = audio.CreatePipeline(lis_conf)
		} else {
			err = fmt.Errorf("unimplemented listener")
		}

		if err != nil {
			fmt.Printf("%s\n",err.Error());
		} else {
			Lists = append(Lists, Lis)
		}
	}

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

	_hid := hid.NewHIDSingleton(URL)
	go func() {
		for {
			channel := chans.Confs["hid"]
			if channel != nil {
				str := <-chans.Confs["hid"].Recv
				_hid.ParseHIDInput(str)
			} else {
				return
			}
		}
	}()



	prox, err := proxy.InitWebRTCProxy(nil, &grpc, &rtc, br, &chans, Lists)
	if err != nil {
		fmt.Printf("%s\n",err.Error())
		return;
	}
	<-prox.Shutdown
}

