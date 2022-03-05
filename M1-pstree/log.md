#### M1
##### 问题
1. 在64位机上编译32位的bianry没有相应的头文件和库文件，在askubuntu上找到了安装  
   的办法。

   ```shell
   sudo apt install libc6-dev-i386
   ```
   
   [How to compile 32-bit executable on 64 bit system](https://askubuntu.com/questions/1270351/how-to-compile-32-bit-executable-on-64-bit-system)


2. 解析`/proc/[pid]/stat`这个文件的内容时，没有发现第二个字段`comm`中也可能会包  
   含`whitespace`，手动改了一下读出来的字符串，将括号外的空白换为`%`，然后再调  
   用`strtok`用`%`来split

3. 发现不止有`systemd`一个进程的ppid是0，还有一个`kthreadd`，谷歌了一下，发现了  
   这个[问题](https://stackoverflow.com/questions/60410486/is-kthreadd-included-in-the-linux-processes)
   结果就是忽视这个家伙，前序遍历时直接从`systemd`开始就可以了

4. `pstree`还会打印线程，如这个[问题](https://stackoverflow.com/questions/25567408/why-are-some-processes-shown-in-pstree-not-shown-in-ps-ef)所说。
   实验没有要求这个，只打印出进程就好
