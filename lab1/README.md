# Linux内核裁剪记录
初始Linux内核：11.5MB;
去掉第一层中的virtualization，networking support ，loaded module：7.4MB;
再次去掉file system中的全部：7.0MB;
去掉kernel hacking中的全部： 6.6MB;
去掉device 中的LED之类的无用设备： 4.5MB；
再次去掉第一层中可去除的剩余部分以及第二层中的一部分： 3.7MB.
