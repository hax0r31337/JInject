# JInject
Injectable java debugger for linux

## Compiling
**Note:** _Do NOT download or compile as the root user._

If you are having problems compiling make sure you've got the latest version of `g++`.

[How to update g++](https://github.com/LWSS/Fuzion/wiki/Updating-your-compiler)

==================

__Ubuntu-Based / Debian:__
```bash
sudo apt-get install cmake g++ gdb git libsdl2-dev zlib1g-dev patchelf libglfw3-dev 
```
__Arch:__
```bash
sudo pacman -S base-devel cmake gdb git sdl2 patchelf glfw-x11
```
__Fedora:__
```bash
sudo dnf install cmake gcc-c++ gdb git libstdc++-static mesa-libGL-devel SDL2-devel zlib-devel libX11-devel patchelf
```

then Compile with build script.  
~~~
./neko -b
~~~

## Debugger
### with Transformer
~~~java
import me.kotone.jisdk.TransformerManager; // add sdk to your jar

public class TheClass {

    public static Class[] main(TransformerManager tmngr) {
        tmngr.addTransformer(new TheTransformer());
        return new Class[]{WantsToRetransform.class};
    }
}

import me.kotone.jisdk.ClassFileTransformer;

public class TheTransformer implements ClassFileTransformer {

    @Override
    public byte[] transform(String classname, byte[] bufferToUse) {
        return bufferToUse;
    }
}
~~~
and add key with `JIAgent-Class` key with your class package.name in to make injector can locate your class.  

### with Main Class
just like how normal Main-Class is