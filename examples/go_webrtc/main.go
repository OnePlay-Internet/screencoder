package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"bytes"
	"os"
	"time"

	proxy "github.com/OnePlay-Internet/webrtc-proxy"
	"github.com/OnePlay-Internet/webrtc-proxy/listener"
	appsink "github.com/Oneplay-Internet/screencoder/sink/appsink/go"

	"github.com/OnePlay-Internet/webrtc-proxy/listener/audio"
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
		} else if lis_conf.MediaType == "audio" {
			Lis = audio.CreatePipeline(lis_conf)
		} else {
			err = fmt.Errorf("unimplemented listener")
		}

		if err != nil {
			fmt.Printf("%s\n",err.Error());
		} else {
			Lists = append(Lists, Lis)
		}
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
					// str := <-chans.Confs["hid"].Recv
					// go ParseHIDInput(str)
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





/*
 * HID code
 */
const (
	HIDproxyEndpoint = "localhost:5000"
)

const (
	mouseWheel = 0
	mouseMove = 1
	mouseBtnUp = 2
	mouseBtnDown = 3
	
	keyUp = 4
	keyDown = 5
	keyPress = 6
	keyReset = 7
)


type HIDMsg struct {
	EventCode int			`json:"code"`
	Data map[string]interface{} `json:"data"`
}

func ParseHIDInput(data string) {
	var err error;
	var route string;
	var out []byte;

	bodymap := make(map[string]float64)
	var bodyfloat float64
	var bodystring string 
	bodyfloat = -1;
	bodystring = "";

	var msg HIDMsg;
	json.Unmarshal([]byte(data),&msg);
	json.Unmarshal([]byte(data),&msg);
	switch msg.EventCode {
	case mouseWheel:
		route = "Mouse/Wheel"
		bodyfloat = msg.Data["deltaY"].(float64);
	case mouseBtnUp:
		route = "Mouse/Up"
		bodyfloat = msg.Data["button"].(float64);
	case mouseBtnDown:
		route = "Mouse/Down"
		bodyfloat = msg.Data["button"].(float64);
	case mouseMove:
		route = "Mouse/Move"
		bodymap["X"] = msg.Data["dX"].(float64);
		bodymap["Y"] = msg.Data["dY"].(float64);

	case keyUp:
		route = "Keyboard/Up"
		bodystring = msg.Data["key"].(string);
	case keyDown:
		route = "Keyboard/Down"
		bodystring = msg.Data["key"].(string);

	case keyReset:
		route = "Keyboard/Reset"
	case keyPress:
		route = "Keyboard/Press"
	}

	
	if bodyfloat != -1 {
		out,err = json.Marshal(bodyfloat)
	} else if bodystring != "" {
		out,err = json.Marshal(bodystring)
	} else if len(bodymap) != 0 {
		out,err = json.Marshal(bodymap)
	} else {
		out = []byte("");
	}

	if err != nil {
		fmt.Printf("fail to marshal output: %s\n",err.Error());
	}
	_,err = http.Post(fmt.Sprintf("http://%s/%s",HIDproxyEndpoint,route),
		"application/json",bytes.NewBuffer(out));
	if err != nil {
		fmt.Printf("fail to forward input: %s\n",err.Error());
	}
}