#include <jni.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "mud.h"

extern int dbsc_original_main(int argc, char **argv);
extern bool mud_down;

JNIEXPORT jint JNICALL
Java_com_skyfire_dbsc_NativeBridge_runServer(JNIEnv *env, jclass cls, jstring rootPath, jint port) {
    const char *root = (*env)->GetStringUTFChars(env, rootPath, 0);
    char cwd[4096];
    snprintf(cwd, sizeof(cwd), "%s/src", root);
    if (chdir(cwd) != 0) {
        (*env)->ReleaseStringUTFChars(env, rootPath, root);
        return -2;
    }
    char portbuf[16];
    snprintf(portbuf, sizeof(portbuf), "%d", (int)port);
    char *argv[] = { "dbsaga", portbuf, NULL };
    (*env)->ReleaseStringUTFChars(env, rootPath, root);
    return dbsc_original_main(2, argv);
}

JNIEXPORT void JNICALL
Java_com_skyfire_dbsc_NativeBridge_stopServer(JNIEnv *env, jclass cls) {
    mud_down = TRUE;
}
