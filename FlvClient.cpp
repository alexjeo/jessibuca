#include "base.h"
#include "FlvDecoder.h"
int main()
{
    EM_ASM(
	var fc = window["FlvClient"] = Module["FlvClient"];
	window.AudioContext = window.AudioContext || window.webkitAudioContext;
	var context = new window.AudioContext();
	if (/(iPhone|iPad|iPod|iOS|Safari)/i.test(navigator.userAgent)) {
		function _unlock(){
			context.resume();
			var source = context.createBufferSource();
			source.buffer = context.createBuffer(1, 1, 22050);
			source.connect(context.destination);
			source.start(0);
		}
		document.addEventListener("mousedown", _unlock, true);
		document.addEventListener("touchend", _unlock, true);
	}
	
	fc.prototype.checkVideoBuffer = function(t) {
		return setTimeout(this.decodeVideoBuffer.bind(this), t);
	};
	fc.prototype.onNetStatus = function(info){

	};
	fc.prototype.initAudio = function(frameCount, samplerate, channels) {
			var isPlaying = false;
			var audioBuffers = [];
			var allFrameCount = frameCount*channels;
			var audioBufferTime = frameCount * 1000 /samplerate;
			//var AudioContext = window.AudioContext || window.webkitAudioContext || window.mozAudioContext || window.msAudioContext;
			//var audioCtx = this.audioContext;//new AudioContext();
			if (!context)return false;
			var setCurrentAudioData;
			var resampled = samplerate < 22050;
			var audioBuffer = resampled? context.createBuffer(channels, frameCount<<1, samplerate<<1) : audioCtx.createBuffer(channels, frameCount, samplerate);
			var outputPtr = this._initAudio(frameCount, channels);
			var audioOutputArray = HEAP16.subarray(outputPtr, outputPtr + allFrameCount);
			var playNextBuffer = function() {
				isPlaying = false;
				if (audioBuffers.length) {
					playAudio(audioBuffers.shift());
				}
				if(audioBuffers.length>1)audioBuffers.shift();
				//console.log(audioBuffers.length)
			};
			var copyAudioOutputArray = resampled ? function(target)
			{
				for (var i = 0; i < allFrameCount; i++) {
					var j = i << 1;
					target[j]  = target[j+1] = audioOutputArray[i] / 32768;
				}
			} : function(target)
			{
				for (var i = 0; i < allFrameCount; i++) {
					
					target[i] = audioOutputArray[i] / 32768;
				}
			};
			var copyToCtxBuffer =channels>1? function(fromBuffer)
			{
				for (var channel = 0; channel < channels; channel++)
				{
					var nowBuffering = audioBuffer.getChannelData(channel);
					if (fromBuffer)
					{
						for (var i = 0; i < frameCount; i++) {
							nowBuffering[i] = fromBuffer[i* (channel + 1)];
						}
					}
					else
					{
						for (var i = 0; i < frameCount; i++) {
							nowBuffering[i] = audioOutputArray[i* (channel + 1)] / 32768;
						}
					}
				}
			}:function(fromBuffer)
			{
				var nowBuffering = audioBuffer.getChannelData(0);
				if (fromBuffer)nowBuffering.set(fromBuffer);
				else copyAudioOutputArray(nowBuffering);
			};
			var playAudio = function(fromBuffer) {
				if (isPlaying) {
					var buffer = new Float32Array(resampled ? allFrameCount *2: allFrameCount);
					copyAudioOutputArray(buffer);
					audioBuffers.push(buffer);
					return;
				}
				isPlaying = true;
				copyToCtxBuffer(fromBuffer);
				var source = context.createBufferSource();
				source.buffer = audioBuffer;
				source.connect(context.destination);
				source.onended = playNextBuffer;
				//setTimeout(playNextBuffer, audioBufferTime-audioBuffers.length*200);
				source.start();
			};
			this.playAudio = playAudio; 
		};
		fc.prototype.play = function(url,webGLCanvas) {
			
			var canvas = webGLCanvas.canvasElement;
			this.setVideoSize = function(w, h, dataPtr)
			{
				canvas.width = w;
				canvas.height = h;
				if (webGLCanvas.isWebGL())
				{
					this.draw = function() {
						var y = HEAPU32[dataPtr];
						var u = HEAPU32[dataPtr + 1];
						var v = HEAPU32[dataPtr + 2];
						var outputArray = [HEAPU8.subarray(y, y + w*h), HEAPU8.subarray(u, u + (w*h >> 2)), HEAPU8.subarray(v, v + (w*h >> 2))];
						webGLCanvas.drawNextOutputPicture(w, h, null, outputArray);
					};
				}else
				{
					var outputArray = HEAPU8.subarray(dataPtr, dataPtr + (w*h << 2));
					webGLCanvas.initRGB(w, h);
					this.draw = function() {
						webGLCanvas.drawNextOutputPicture(w, h, null, outputArray);
					};
				}
			}.bind(this);
			
			this.getWebGL = function()
			{
				return webGLCanvas;
			};
			var _this = this;
			var ws = new WebSocket(url);
			ws.onmessage = function(data){
				_this.$onWsMessage(data);
			};
			ws.onopen = function(){
				Module.print("websocket open");
			};
			ws.binaryType = "arraybuffer";
			if(this.onWsError)ws.onerror = this.onWsError;
			ws.onclose = function(){
				_this.$close();
			};
			this.close = function(){
				ws.close();
			};
			this.$play(this, webGLCanvas.isWebGL());
		};
	);
    return 0;
}
class FlvClient
{

	val* client = nullptr;
	val* netStatusLisenter = nullptr;
	val* jsThis = nullptr;
	string url;
	int status = 0;
	int audioBuffer = 12;
	MemoryStream buffer;
public:
	FlvDecoder flvDecoder;
	FlvClient(){
		
    }
	~FlvClient(){
      
        delete client;
        delete netStatusLisenter;
        delete jsThis;
    }
	void OnWsMessage(val evt){
        string data = evt["data"].as<string>();
		switch(status){
			case 0:
				buffer<<data;
				if(buffer.length()>=13){
					status=1;
					buffer.offset=13;
					buffer.removeConsume();
					
				}
				break;
			case 1:
				buffer<<data;
				while(buffer.length()>3)
				{
					u8 type = buffer.readu8();
					
					unsigned int length = buffer.readUInt24B();
					if(buffer.length()<length+4+7){
						buffer<<=4;
						break;
					}
					unsigned int timestamp = buffer.readUInt24B();
					u8 ext = buffer.readu8();
					buffer.readUInt24B();
					MemoryStream ms(buffer.readString(length));
					switch(type){
						case 0x08:
						if(!flvDecoder.audioDecoder){
							unsigned char flag = ms[0];
							auto audioType = flag >> 4;
							int channels = (flag & 1)+1;
							int rate = (flag>>2)&3;
							
							switch(rate){
								case 1:
								rate=11025;
								break;
								case 2:
								rate=22050;
								break;
								case 3:
								rate=44100;
								break;
							}
							switch(audioType){
								case 10://AAC
								jsThis->call<void>("initAudio",audioBuffer * 1024, rate, channels);
								break;
								case 11://Speex
								jsThis->call<void>("initAudio",50*320, 16000 , channels);
								break;
								case 2://MP3
								jsThis->call<void>("initAudio",audioBuffer * 576, rate,channels);
								break;
							}
						}
						flvDecoder.decodeAudio(timestamp,ms);
						break;
						case 0x09:
						flvDecoder.decodeVideo(timestamp,ms);
						break;
					}
					length = buffer.readUInt32B();
				
				}
				buffer.removeConsume();
				break;
		}
        //MemoryStream ms(data);
    }
	void Play(val _this, bool webgl){
         if (!jsThis) jsThis = new val(_this);
		 flvDecoder.attachCanvas(jsThis,webgl);
    }
	void Close(){
		buffer.clear();
		flvDecoder.clear();
		status = 0;
        //delete this;
    }
	int initAudio(int frameCount, int channels) {
		emscripten_log(0,"%d,%d",frameCount,channels);
		return flvDecoder.initAudio(frameCount,channels);
	}
	void decodeVideoBuffer(){
		flvDecoder.decodeVideoBuffer();
	}
	val getBufferTime() const
	{
		return val(flvDecoder.bufferTime);
	}
	void setBufferTime(val value)
	{
		flvDecoder.bufferTime = value.as<int>();
	}
	val getAudioBuffer() const
	{
		return val(audioBuffer);
	}
	void setAudioBuffer(val value)
	{
		audioBuffer = value.as<int>();
	}
};
EMSCRIPTEN_BINDINGS(FlvClient)
{
    class_<FlvClient>("FlvClient")
	.constructor()
	.function("$onWsMessage", &FlvClient::OnWsMessage)
	.function("$close", &FlvClient::Close)
	.function("_initAudio",&FlvClient::initAudio)
	.function("decodeVideoBuffer", &FlvClient::decodeVideoBuffer)
	.property("videoBuffer", &FlvClient::getBufferTime, &FlvClient::setBufferTime)
	.property("audioBuffer", &FlvClient::getAudioBuffer, &FlvClient::setAudioBuffer)
	.function("$play", &FlvClient::Play)
	;
}