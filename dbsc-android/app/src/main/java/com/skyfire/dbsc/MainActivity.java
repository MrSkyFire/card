package com.skyfire.dbsc;

import android.app.*;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicBoolean;

public class MainActivity extends Activity {
    private TextView terminal, status;
    private EditText input;
    private Socket socket;
    private OutputStream out;
    private final AtomicBoolean serverStarted = new AtomicBoolean(false);

    @Override public void onCreate(Bundle b) {
        super.onCreate(b);
        buildUi();
        new Thread(() -> {
            try {
                File root = new File(getFilesDir(), "dbsc");
                copyAssetTree("dbsc", root);

                // DBSC expects to run from <root>/src so its historical ../area,
                // ../system, ../player, etc. relative paths resolve correctly.
                // Android APK assets do not preserve empty directories, so create it.
                File runtimeSrc = new File(root, "src");
                if (!runtimeSrc.exists() && !runtimeSrc.mkdirs()) {
                    throw new IOException("could not create runtime src directory: " + runtimeSrc);
                }

                if (serverStarted.compareAndSet(false, true)) {
                    runOnUiThread(() -> status.setText("Starting server on 127.0.0.1:4000..."));
                    new Thread(() -> {
                        int rc = NativeBridge.runServer(root.getAbsolutePath(), 4000);
                        append("\n[Server exited] code " + rc + "\n");
                    }, "DBSC-Server").start();
                    Thread.sleep(1600);
                }
                connectLocal();
            } catch (Throwable t) { append("\n[Startup error] " + t + "\n"); }
        }, "DBSC-Bootstrap").start();
    }

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(12,12,12,12);
        root.setBackgroundColor(0xff000000);

        status = new TextView(this);
        status.setText("Preparing DBSC...");
        status.setTextColor(0xff80ff80);
        root.addView(status, new LinearLayout.LayoutParams(-1,-2));

        ScrollView scroll = new ScrollView(this);
        terminal = new TextView(this);
        terminal.setTextColor(0xffdddddd);
        terminal.setTextSize(14);
        terminal.setTextIsSelectable(true);
        scroll.addView(terminal);
        root.addView(scroll, new LinearLayout.LayoutParams(-1,0,1));

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        input = new EditText(this);
        input.setSingleLine(true);
        input.setTextColor(0xffffffff);
        input.setHintTextColor(0xff777777);
        input.setHint("command");
        row.addView(input, new LinearLayout.LayoutParams(0,-2,1));
        Button send = new Button(this); send.setText("Send");
        row.addView(send, new LinearLayout.LayoutParams(-2,-2));
        root.addView(row);

        send.setOnClickListener(v -> sendLine());
        input.setOnEditorActionListener((v,a,e) -> { sendLine(); return true; });
        setContentView(root);
    }

    private void connectLocal() {
        try {
            for (int i=0;i<20;i++) {
                try { socket = new Socket("127.0.0.1", 4000); break; }
                catch (IOException e) { Thread.sleep(300); }
            }
            if (socket == null || !socket.isConnected()) throw new IOException("server did not open port 4000");
            out = socket.getOutputStream();
            runOnUiThread(() -> status.setText("DBSC running • local client connected"));
            InputStream in = socket.getInputStream();
            ByteArrayOutputStream line = new ByteArrayOutputStream();
            while (!socket.isClosed()) {
                int b = in.read(); if (b < 0) break;
                if (b == 255) { // TELNET IAC: skip command + option for simple negotiations
                    int cmd = in.read(); if (cmd < 0) break;
                    if (cmd != 255) { int opt = in.read(); if (opt < 0) break; }
                    continue;
                }
                if (b == 0) continue;
                line.write(b);
                if (b == '\n' || line.size() > 2048) {
                    String s = line.toString("UTF-8").replaceAll("\\u001B\\[[;\\d]*[ -/]*[@-~]", "");
                    line.reset(); append(s);
                }
            }
        } catch (Throwable t) { append("\n[Client error] " + t + "\n"); }
    }

    private void sendLine() {
        String s=input.getText().toString(); input.setText("");
        try { if(out!=null){ out.write((s+"\r\n").getBytes(StandardCharsets.UTF_8)); out.flush(); } }
        catch(IOException e){ append("\n[send failed] "+e+"\n"); }
    }

    private void append(String s) {
        runOnUiThread(() -> {
            terminal.append(s);
            ((ScrollView)terminal.getParent()).post(() -> ((ScrollView)terminal.getParent()).fullScroll(View.FOCUS_DOWN));
        });
    }

    private void copyAssetTree(String assetPath, File dst) throws IOException {
        String[] kids=getAssets().list(assetPath);
        if(kids==null || kids.length==0){
            dst.getParentFile().mkdirs();
            try(InputStream in=getAssets().open(assetPath); OutputStream o=new FileOutputStream(dst)){
                byte[] buf=new byte[65536]; int n; while((n=in.read(buf))>0)o.write(buf,0,n);
            }
            return;
        }
        dst.mkdirs();
        for(String k:kids) copyAssetTree(assetPath+"/"+k,new File(dst,k));
    }

    @Override protected void onDestroy() {
        try { if(socket!=null) socket.close(); } catch(Exception ignored){}
        NativeBridge.stopServer();
        super.onDestroy();
    }
}
