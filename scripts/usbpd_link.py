Import("env")

env.Append(LIBS=[env.File("$PROJECT_DIR/lib/UsbPdST/lib/USBPDCORE_PD3_CONFIG_MINSNK_CM4_wc32.a")])
