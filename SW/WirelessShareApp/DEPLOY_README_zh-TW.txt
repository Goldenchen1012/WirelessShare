WirelessShareApp 使用說明
=========================

系統需求：64-bit Windows 10 或 Windows 11。

v1.0.8 新增 Silicon Labs CP210x（VID 10C4、PID EA60）直接有線串列模式，
並保留原有 LOLIN S2 Wi-Fi 無線模式。直接串列模式使用 921600 baud、8N1、無硬體流量控制，
也會使用資料幀 ACK/NACK 與 sequence 流量控制。

LOLIN S2 Wi-Fi 模式：
1. 將整個資料夾解壓縮，不要只單獨取出 EXE。
2. 插入已燒錄 WirelessDevice 韌體的 LOLIN S2 Mini。
3. 執行 WirelessShareApp.exe。
4. 選擇 LOLIN S2 的 USB CDC COM 埠。
5. 其中一台電腦選 A－無線基地台，另一台選 B－連線端。
6. 兩台電腦輸入完全相同的 8～63 bytes 配對密碼，再按「連線」。
7. 雙方都顯示「已與對方連線」後，即可複製文字、圖片、檔案或資料夾。

CP210x 直接有線串列模式：
1. A、B 電腦各插入一個 CP210x USB to UART 裝置。
2. 兩個 CP210x 只連接三條線：A TX 接 B RX、A RX 接 B TX、A GND 接 B GND。
3. 不要連接兩個模組的 5V、3.3V 或 VCC 腳位。
4. 兩台電腦執行 WirelessShareApp.exe，並各自選擇顯示為 CP210x 的 COM 埠。
5. 一台選「A－直接串列端點」，另一台選「B－直接串列端點」。
6. 輸入完全相同的 8～63 bytes 配對密碼，再按「連線」。
7. 雙方都顯示「CP210x 直接串列：已與對方連線」後即可傳送。

CP210x 模式不需要 LOLIN S2，也不需要燒錄韌體。若 Windows 裝置管理員未出現
「Silicon Labs CP210x USB to UART Bridge」，請先安裝 Silicon Labs CP210x VCP 驅動程式。

收到的檔案預設儲存在：下載\WirelessShare

注意事項：
- 兩台電腦必須分別使用 A 與 B 角色，不可同時選 A 或同時選 B。
- 配對密碼不同時無法連線。
- App 會固定等待原本選定的 USB 裝置，不會在裝置重新連線時誤開 COM1。
- 狀態列中的 reset、disconnect、wifi_retry、tcp_retry 是裝置診斷數值。
- usb_rx_drop、usb_tx_fail、usb_crc_fail 與 tcp_tx_fail 正常應持續為 0；若數值增加，請保留兩端傳輸記錄以便查修。
- 關閉主視窗後程式仍會在 Windows 系統匣執行。
- 請保留本資料夾內所有 DLL 與 plugins 子目錄。
- CP210x 接線必須交叉 TX/RX，並共接 GND；請勿連接兩端電源腳位。
- 本版本已完成軟體編譯與通訊協定測試，仍需使用兩台實體電腦與兩個 CP210x 完成硬體驗證。
