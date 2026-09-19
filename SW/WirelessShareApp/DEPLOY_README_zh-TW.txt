WirelessShareApp 使用說明
=========================

系統需求：64-bit Windows 10 或 Windows 11。

使用步驟：
1. 將整個資料夾解壓縮，不要只單獨取出 EXE。
2. 插入已燒錄 WirelessDevice 韌體的 LOLIN S2 Mini。
3. 執行 WirelessShareApp.exe。
4. 選擇 LOLIN S2 的 USB CDC COM 埠。
5. 其中一台電腦選 A－無線基地台，另一台選 B－連線端。
6. 兩台電腦輸入完全相同的 8～63 bytes 配對密碼，再按「連線」。
7. 雙方都顯示 Peer connected 後，即可複製文字、圖片、檔案或資料夾。

收到的檔案預設儲存在：下載\WirelessShare

注意事項：
- 兩台電腦必須分別使用 A 與 B 角色，不可同時選 A 或同時選 B。
- 配對密碼不同時無法連線。
- 關閉主視窗後程式仍會在 Windows 系統匣執行。
- 請保留本資料夾內所有 DLL 與 plugins 子目錄。
- 此程式尚未進行兩台實體電腦與兩塊 LOLIN S2 的完整硬體驗證。
