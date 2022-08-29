package main

import (
	"github.com/OnePlay-Internet/webrtc-proxy/listener"
	"github.com/OnePlay-Internet/webrtc-proxy/util/config"
	appsink "github.com/Oneplay-Internet/screencoder/sink/appsink/go"
	"github.com/pion/rtp"
	"github.com/pion/webrtc/v3/pkg/media"
)



type ScreencoderListener struct {
	sink *appsink.Appsink
	conf config.ListenerConfig
}


func NewScreencoderListener(conf config.ListenerConfig) listener.Listener {
	ret := ScreencoderListener{}
	ret.conf = conf;
	ret.sink = appsink.NewAppsink();
	return &ret;
}



func (lis *ScreencoderListener) ReadConfig() *config.ListenerConfig {
	return &lis.conf;
}

func (lis *ScreencoderListener) ReadRTP() *rtp.Packet {
	return lis.sink.ReadRTP()
}

func (lis *ScreencoderListener) ReadSample() *media.Sample {
	chn := make(chan *media.Sample);
	return <-chn
}

func (lis *ScreencoderListener) Open(){
}

func (lis *ScreencoderListener) OnClose(fun listener.OnCloseFunc) {
}

func (lis *ScreencoderListener) Close() {
}