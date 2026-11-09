// core.h
// 作者(Suguru Kawamoto)オリジナルの dpeaklim.cpp / InpOut32.cpp に実装された
// 関数群を GUI 側から呼び出すための宣言のみ（ロジックは一切ここに無い）。
#pragma once
#include <windows.h>

// dpeaklim.cpp 内の構造体と同一レイアウト（宣言の重複は ODR 上問題なし）
struct PHYSICAL_MEMORY_PATCH
{
    DWORD FirstBits;
    BYTE  Hash[16];
    DWORD Offset;
    DWORD Mask;
    DWORD Data;
};

// --- 以下はすべて作者オリジナル実装（dpeaklim.cpp / InpOut32.cpp）---
BOOL Load();                               // inpoutx64.dll ドライバを開く
void Unload();                             // ドライバを閉じる
BOOL RestartWindowsAudioService();         // AudioSrv を停止→開始
BOOL GenerateDisableAudioEngineLimiterPatch(PHYSICAL_MEMORY_PATCH* pPatch, DWORD* pCount,
                                            FLOAT Original, FLOAT Overriding, BOOL bRestore);
// Force mode（実験的・ビルド依存）: mov [reg+0C8h],1.0f の即値を Overriding(+∞) に書き換える
BOOL GenerateForceDisableLimiterPatch(PHYSICAL_MEMORY_PATCH* pPatch, DWORD* pCount,
                                      FLOAT Overriding, BOOL bRestore);
BOOL ApplyPatch(PHYSICAL_MEMORY_PATCH* pPatch, DWORD Count);
// Verify用: 指定パッチに一致するページが物理メモリに常駐している数（読み取り専用）
DWORD CountPatchedPages(PHYSICAL_MEMORY_PATCH* pPatch, DWORD Count);
BOOL DisableAudioEngineLimiter(FLOAT Original, FLOAT Overriding, BOOL bRestore);
