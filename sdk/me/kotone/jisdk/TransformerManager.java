package me.kotone.jisdk;

/**
 * Support class for the InstrumentationImpl. Manages the list of registered transformers.
 * Keeps everything in the right order, deals with sync of the list,
 * and actually does the calling of the transformers.
 */
public class TransformerManager {
    /**
     * a given instance of this list is treated as immutable to simplify sync;
     * we pay copying overhead whenever the list is changed rather than every time
     * the list is referenced.
     * The array is kept in the order the transformers are added via addTransformer
     * (first added is 0, last added is length-1)
     * Use an array, not a List or other Collection. This keeps the set of classes
     * used by this code to a minimum. We want as few dependencies as possible in this
     * code, since it is used inside the class definition system. Any class referenced here
     * cannot be transformed by Java code.
     */
    private ClassFileTransformer[] mTransformerList = new ClassFileTransformer[0];

    public synchronized void
    addTransformer(ClassFileTransformer transformer) {
        ClassFileTransformer[] oldList = mTransformerList;
        ClassFileTransformer[] newList = new ClassFileTransformer[oldList.length + 1];
        System.arraycopy(oldList,
                0,
                newList,
                0,
                oldList.length);
        newList[oldList.length] = transformer;
        mTransformerList = newList;
    }

    public byte[] transform(String classname, byte[] classfileBuffer) {
        boolean someoneTouchedTheBytecode = false;

        byte[] bufferToUse = classfileBuffer;

        // order matters, gotta run 'em in the order they were added
        for (ClassFileTransformer transformer : mTransformerList) {
            byte[] transformedBytes = null;

            try {
                transformedBytes = transformer.transform(classname, bufferToUse);
            } catch (Throwable t) {
                // don't let any one transformer mess it up for the others.
                // This is where we need to put some logging. What should go here? FIXME
            }

            if (transformedBytes != null) {
                someoneTouchedTheBytecode = true;
                bufferToUse = transformedBytes;
            }
        }

        // if someone modified it, return the modified buffer.
        // otherwise return null to mean "no transforms occurred"
        byte[] result;
        if (someoneTouchedTheBytecode) {
            result = bufferToUse;
        } else {
            result = null;
        }

        return result;
    }


    public int getTransformerCount() {
        return mTransformerList.length;
    }
}
