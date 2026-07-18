# ACLIC 最小模型 — 行為規格(SPEC)

> 範圍:sourcecfg / iprio / setip / setie / ithreshold / 組合仲裁 / claim。
> 刻意排除:indirect CSR 打包位寬、domain、Smnip、eidelivery(單候選,視為恆選 ACLIC)。
> 角色分工:本 SPEC + `test_aclic.c` 是 testbench(不可改);`aclic.cpp` 的 TODO 是 DUT,由妳實作。
> 若妳認為某個測項與本 SPEC 矛盾——挑戰它,那本身就是 DV 的日常。

## 常數與編碼

- `ACLIC_NSRC = 32`;**id 0 保留 =「無中斷」**,有效 source 為 1..31。
- priority 8-bit,**數字越小越優先**。
- `topi` 編碼:`(id << 16) | prio`;無贏家 = 0。
- sourcecfg 的 SM 編碼沿 APLIC:INACTIVE=0, DETACHED=1, EDGE1=4, EDGE0=5, LEVEL1=6, LEVEL0=7。

## 行為規則(實作以此為準,測項以 R 編號回溯)

**R1|線的翻譯(誰能寫 pending 之一:device)**
- `EDGE1`:line 出現 **0→1 跳變**的那一刻,`setip` 置 1。跳變是唯一事件;line 維持高、或 1→0,都不產生 pending。`EDGE0` 鏡像(1→0 觸發)。
- `LEVEL1`:pending 是**鏡子**——line 為 1 期間 `setip` 持續成立;line 落回 0,`setip` 自動清 0。`LEVEL0` 鏡像。
- `INACTIVE`:這條線不存在。line 變化無效,**軟體 set 也無效**,`setip` 恆 0。
- `DETACHED`:線被斷開(line 變化無效),但**軟體可置 pending**(R2)。

**R2|軟體寫 pending(誰能寫之二)**
- `aclic_sw_setip(src, v)`:對 DETACHED / EDGE 類 source,任意 set/clear。
- 對 LEVEL 類:clear 之後若 line 仍使條件成立,pending **立即重建**(鏡子不接受欺騙);set 在條件不成立時允許(注入測試)。
- 對 INACTIVE:無效。

**R3|仲裁(topi 是函數,不是儲存)**
- 參賽資格:`setip[i] ∧ setie[i] ∧ 過門檻(R4)`。
- 勝者:iprio **最小**;同 iprio 時 **id 較小者勝**(本模型的 tiebreak 約定)。
- 無人參賽 → `topi = 0`。`topi` 隨輸入即時重算,呼叫多次不改變任何狀態。

**R4|門檻**
- `ithreshold = 0`:不設限,全放行。
- `ithreshold = P ≠ 0`:**iprio ≥ P 者不得參賽;iprio < P(嚴格小於)放行。**
- 門檻只影響**參賽資格**,不影響儲存:被擋下的 source 其 `setip` 依然為 1(可由 `aclic_peek_ip` 觀察)。

**R5|claim(唯一有副作用的讀)**
- `aclic_claim()` 原子地:回傳**當下** `topi`,並清掉該贏家的 `setip`("if possible")。
- 贏家是 LEVEL 類且條件仍成立(line 還拉著)→ pending 清掉後**立即重建**(R1 的鏡子性質優先)。
- `topi = 0` 時 claim 回傳 0 且**不清任何東西**(spurious)。
- claim 只清贏家一人;其他 pending 不受影響。

**R6|irq_out**
- `aclic_irq_out() = (topi != 0)`。純組合,無記憶。

**R7|reset**
- 全部 sourcecfg = INACTIVE、iprio = 0xFF、setip/setie = 0、ithreshold = 0、line = 0。

## 驗收定義

`make test` 全數 PASS。過程中任何「咦,spec 沒講這個 corner」的瞬間,記下來帶回討論——那份清單是這個練習真正的產出。
