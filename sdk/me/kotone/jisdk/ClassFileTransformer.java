package me.kotone.jisdk;

import java.io.IOException;

public interface ClassFileTransformer {
    byte[] transform(String classname, byte[] bufferToUse);
}
