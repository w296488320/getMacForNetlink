package com.example.jnihook;


import android.app.Activity;
import android.os.Bundle;

public class MainActivity extends Activity {

    static {
        System.loadLibrary("helloword");
    }

    public static native void getmac();
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        getmac();

    }


}