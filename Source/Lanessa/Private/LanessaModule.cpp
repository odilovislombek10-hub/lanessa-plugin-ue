// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "LanessaRemoteSettings.h"

#if WITH_EDITOR
#include "Editor/EditorPerformanceSettings.h"
#endif

/**
 * Plagin moduli. Ishga tushganda editorning "Use Less CPU when in Background"
 * sozlamasini o'chiradi, chunki masofadan boshqarish aynan shunga urilib qotadi.
 *
 * Remote Control buyruqni KADRGA BITTA bajaradi, ya'ni editorning kadr tezligi -
 * API ning o'tkazuvchanlik chegarasi. Telefondan boshqarilganda editor oynasi hech
 * qachon fokusda bo'lmaydi, shuning uchun u ~3 FPS ga tushadi va boshqarish paytida
 * qaltirab qotadi: o'lchangan - bitta chaqiruv 333 ms, o'chirilgandan keyin 16 ms.
 *
 * Buni plaginning o'zi qiladi, shuning uchun EditorSettings.ini ni qo'lda tahrirlash
 * kerak emas va plagin ko'chirilganda sozlama ham u bilan ketadi. O'chirib qo'yish
 * uchun: Project Settings -> Plugins -> Lanessa Remote -> "Disable Background Throttle".
 */
class FLanessaModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		if (!ULanessaRemoteSettings::Get().bDisableBackgroundThrottle)
		{
			return;
		}
		UEditorPerformanceSettings* Perf = GetMutableDefault<UEditorPerformanceSettings>();
		if (!Perf || !Perf->bThrottleCPUWhenNotForeground)
		{
			return;
		}
		Perf->bThrottleCPUWhenNotForeground = false;
		// Darhol saqlaymiz: aks holda editor yopilgunicha faqat xotirada qoladi va
		// keyingi ochilishda yana yoqilgan bo'lib chiqadi.
		Perf->SaveConfig();
		UE_LOG(LogTemp, Log,
			TEXT("[Lanessa] 'Use Less CPU when in Background' o'chirildi - masofadan "
			     "boshqarish kadrga bitta buyruq bajaradi va fokussiz editor ~3 FPS ga tushadi."));
#endif
	}
};

IMPLEMENT_MODULE(FLanessaModule, Lanessa)
