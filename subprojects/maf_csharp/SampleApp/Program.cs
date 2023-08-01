using maf;

namespace SampleApp
{
    class Program
    {
        static void Main(string[] args)
        {
            int byteLength = 1080*1920*3;
            BufferHandler bh = new BufferHandler();
            byte count = 2;
            bh.allocate(count);
            byte i = 0;
            foreach (Frame frame in bh.getFrames()){
                frame->index = i;
                frame->data = maf.maf.new_byteArray(byteLength);
                i++;
            }

            MediaInfo mediaInfo = new MediaInfo();
            AlternativeLocation ml = new AlternativeLocation();
            ml.uri = "C:\\Users\\nlsdv\\Videos\\testsrc.mp4";
            mediaInfo.alternatives.Add(ml);

            BufferInfo b = new BufferInfo();
            b.componentType = ComponentType.UNSIGNED_BYTE;
            b.type = SampleType.VEC3;
            b.attributeType = AttributeType.ATTRIB_COLOR;
            b.offset = 0;
            b.stride = 0;
            b.handler = bh;
            BufferInfoArray buffers = new BufferInfoArray();
            buffers.Add(b);
            
            MediaPipelineFactory factory = new MediaPipelineFactory();
            IMediaPipeline mp = factory.createMediaPipeline(mediaInfo, buffers);
            b = buffers.ElementAt(0);
            TimeInfo ti = new TimeInfo();
            ViewInfo vi = new ViewInfo();
            mp.startFetching(ti, vi);
            
            int read = 0;
            while (mp.state() == PipelineState.ACTIVE)
            {
                Frame f = bh.readFrame();
                if(f != null){
                    read++;
                    Console.WriteLine(String.Format("frame[{0}] idx:{1} pts:{2} size:{3}\n", read, f.index, f.timestamp, f.length));
                }
            }
            mp.stopFetching();
        }
    }
}