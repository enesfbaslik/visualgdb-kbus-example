
//-----------------------------------------------------------------------------
///  \file     potDegerOkuma.c
///
///  \version  v.1.0.1
///
///  \brief    Visual Studio ve VisualGDB kullanarak basit bir analog deger okuma ve yazma programi 
///
///  \author   Enes Furkan Baslik
//----------------------------------------------------------------------------- 
// standard kütüphaneler
//-----------------------------------------------------------------------------

#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
//-----------------------------------------------------------------------------
//  KBUS WAGO ADI icin dahil edilen dosyalar
//-----------------------------------------------------------------------------
#include <dal/adi_application_interface.h>


// oncelikler
#define KBUS_MAINPRIO 40       // ana dongu

//-----------------------------------------------------------------------
/// MAIN
//-----------------------------------------------------------------------

int
main(void)
{
	// ADI-arayuzu icin degiskenler
	tDeviceInfo deviceList[10];             // ADI tarafindan verilen cihaz listesi
	size_t nrDevicesFound;                  // bulunan cihaz sayisi
	size_t nrKbusFound;                     // kbus'in listedeki sirasi
	tDeviceId kbusDeviceId;                 // ADI tarafýndan verilen cihazin adi
	tApplicationDeviceInterface* adi;      // uygulama arayüzünün adresi
	uint32_t taskId = 0;                    // görev ID 
	tApplicationStateChangedEvent event;    // ADI arayüzü icin degisken

	// islem dizisi
	uint16_t pd_in[4096];       // kbus giris islemleri dizisi (WORDS)
	uint16_t pd_out[4096];      // kbus cikis islemleri dizisi (WORDS)

	// generic vars
	int i = 0, loops = 0;
	time_t last_t = 0, new_t;
	long unsigned runtime = 0;
	struct sched_param s_param;

	// baslangic bilgisi */
	printf("|---------------------------------------------------------------------|\n");
	printf("|      VisualGDB ile KBUS Analog Deger Okuyup Yazdirma                |\n");
	printf("|---------------------------------------------------------------------|\n");

	// islem hafizasini temizle
	memset(pd_in, 0, sizeof(pd_in));
	memset(pd_out, 0, sizeof(pd_out));

	// ADI-arayuzune baglan
	adi = adi_GetApplicationInterface();

	// arayuzu baslat
	adi->Init();

	// cihazlarý tara
	adi->ScanDevices();
	adi->GetDeviceList(sizeof(deviceList), deviceList, &nrDevicesFound);

	// kbus cihaz bul
	nrKbusFound = -1;
	for (i = 0; i < nrDevicesFound; ++i)
	{
		if (strcmp(deviceList[i].DeviceName, "libpackbus") == 0)
		{
			nrKbusFound = i;
			printf("Device %i olarak KBUS cihazi bulundu\n", i);
		}
	}

	// kbus bulunamadi > cik
	if(nrKbusFound == -1)
	{
		printf("KBUS cihazi bulunamadi \n");
		adi->Exit();    // ADI-arayuzunden cikis
		return - 1;    // programdan cikis
	}

	// Runtime önceligini degistir
	s_param.sched_priority = KBUS_MAINPRIO;
	sched_setscheduler(0, SCHED_FIFO, &s_param);
	printf("Oncelik degisti 'KBUS_MAINPRIO'\n");

	// kbus cihazini ac
	kbusDeviceId = deviceList[nrKbusFound].DeviceId;
	if (adi->OpenDevice(kbusDeviceId) != DAL_SUCCESS)
	{
		printf("KBUS cihazi acilirken hata\n");
		adi->Exit();    // ADI-arayuzunden cikis
		return - 2;    // programdan cikis
	}
	printf("KBUS cihazi acildi\n");


	// Kbus için kendi kendine aplikasyon durumunu 'Running' yap.
	event.State = ApplicationState_Running;
	if (adi->ApplicationStateChanged(event) != DAL_SUCCESS)
	{
		// Aplikasyon durumunu 'Running' yapmak basarisiz
		printf("Uygulama durumunu 'Running' yapma basarisiz oldu\n");
		adi->CloseDevice(kbusDeviceId);    // kbus cihazýný kapat
		adi->Exit();    // ADI-arayuzunden cikis
		return - 3;    // programdan cikis
	}
	printf("Uygulama durumu 'Running' olarak ayarlandi \n");

	int errorFlag = 0;
    
	// hata kontrolü
	while(errorFlag == 0)
	{
		usleep(100);    // 10ms bekle

		uint32_t retval = 0;

		// "libpackbus_Push" fonksiyonu, kbus cycle triglemesi icin
		if(adi->CallDeviceSpecificFunction("libpackbus_Push", &retval) != DAL_SUCCESS)
		{
			// CallDeviceSpecificFunction baþarýsýz
			printf("CallDeviceSpecificFunction basarisiz\n");
			adi->CloseDevice(kbusDeviceId);    // kbus cihazýný kapat
			adi->Exit();    // ADI-arayuzunden cikis
			errorFlag = -4;
			return -4; // programdan cikis
		}

		if (retval != DAL_SUCCESS)
		{
			// Fonksiyon 'libpackbus_Push' baþarýsýz
			printf("Fonksiyon 'libpackbus_Push' basarisiz\n");
			adi->CloseDevice(kbusDeviceId);    // kbus cihazýný kapat
			adi->Exit();    // ADI-arayuzunden cikis
			errorFlag = -5;
			return -5; // program cikisi
		}

		loops++;

		// Trigger Watchdog
		adi->WatchdogTrigger();

		// test cikisi icin 1sn
		new_t = time(NULL);
		if (new_t != last_t)
		{
			last_t = new_t;
			runtime++;

			// girisleri oku
			adi->ReadStart(kbusDeviceId, taskId); // PD-in verisini kilitle 
			adi->ReadBytes(kbusDeviceId, taskId, 0, 2, (uint16_t*)&pd_in[0]); // adres 0'dan 2 byte oku
			adi->ReadEnd(kbusDeviceId, taskId); // PD-in verisinin kilidini kaldýr 
			
			// analog degeri okuma
			uint16_t analogDeger = (pd_in[0]);
			
			// tekrar yazdýrma için sýfýrlama
			pd_out[0] = 0;
			
			// analog degere gore bit ayari
			
			if (analogDeger > 0){(pd_out[0]  |= 1 << 3); }
			if (analogDeger > 8000){(pd_out[0]  |= 1 << 2); }
			if (analogDeger > 16000){(pd_out[0]  |= 1 << 1); }
			if (analogDeger > 24000){(pd_out[0]  |= 1 << 0); }

			
			// dijital cikisa yazdir
			adi->WriteStart(kbusDeviceId, taskId); // PD-cikis verisini kilitle
			adi->WriteBytes(kbusDeviceId, taskId, 8, 1, (uint8_t*)&pd_out[0]); // yaz
			adi->WriteEnd(kbusDeviceId, taskId); // PD-cikis verisinin kilidini kaldir
			loops = 0;
			// analog okunan degeri goster
			printf("AnalogDeger = %d \n", analogDeger);
		}

	} // while ..

	// kbus cihazýný kapat
	adi->CloseDevice(kbusDeviceId);

	adi->Exit();    // ADI-Interface baðlantýsýný sonlandýrma
	return 0;    // programdan çýkýþ
}//eof
