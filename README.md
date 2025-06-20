# CardReaderApp

## Proje Hakkında
CardReaderApp, staj dönemimde geliştirdiğim, akıllı kart okuyucu cihazları ile haberleşerek kart bilgilerini okuyan ve MIFARE kartlar üzerinde kimlik doğrulama ve blok okuma işlemleri yapabilen bir masaüstü uygulamadır. Proje, Qt5.10.1 framework'ü ve MinGW32 derleyicisi kullanılarak geliştirilmiştir. Uygulama, seri port üzerinden kart okuyucuya bağlanır ve kartlardan gelen verileri kullanıcıya anlaşılır şekilde sunar.

## Amaç ve Kapsam
Bu projenin temel amacı, bir kart okuyucu cihazı ile bilgisayar arasında seri port üzerinden iletişim kurarak kartların temel bilgilerini (tip, UID, SAK, ATQ) ve MIFARE kartlar için kimlik doğrulama ve blok okuma işlemlerini gerçekleştirmektir. Proje, özellikle MIFARE tabanlı kartların güvenli şekilde okunması ve yönetilmesi için örnek bir uygulama sunar.

## Özellikler
- **Seri Port Üzerinden Kart Okuma:** Bilgisayara bağlı kart okuyucu cihazı ile seri port üzerinden iletişim kurar.
- **Port Seçimi ve Yönetimi:** Mevcut seri portları listeler, port açma/kapama işlemlerini yönetir.
- **Kart Bilgisi Okuma:** Kart takıldığında tip, UID, SAK, ATQ gibi temel bilgileri okur ve ekranda gösterir.
- **MIFARE Kimlik Doğrulama:** Kullanıcıdan alınan anahtar tipi, anahtar numarası ve sektör numarası ile MIFARE kartlarda kimlik doğrulama işlemi yapar.
- **MIFARE Blok Okuma:** Belirtilen blok numarasından veri okur ve ekranda gösterir.
- **Ham Veri ve Detaylar:** Karttan gelen ham veriyi ve ayrıntılı bilgileri ayrı bir pencerede görüntüleyebilir.
- **Kapsamlı Hata Yönetimi:** Port, kimlik doğrulama ve blok okuma işlemlerinde detaylı hata mesajları sunar.

## Kullanılan Teknolojiler
- **Qt 5.10.1** (Qt Widgets, Qt SerialPort)
- **MinGW32** (Gömülü derleyici ve debugger)
- **C++**

## Kurulum ve Gereksinimler
- Qt 5.10.1 ve MinGW32 yüklü olmalıdır.
- Proje dosyası (`CardReaderApp.pro`) Qt Creator ile açılabilir.
- Uygulama, Windows ortamında (özellikle Win10) test edilmiştir.
- Kart okuyucu cihazı ve uygun bir MIFARE kart gereklidir.

## Nasıl Çalışır?
1. **Port Seçimi:** Uygulama açıldığında mevcut seri portlar listelenir. Doğru port seçilip "Portu Aç" butonuna basılır.
2. **Kart Okuma:** Kart okuyucuya bir kart yaklaştırıldığında, kartın tipi, UID, SAK ve ATQ bilgileri otomatik olarak ekranda görüntülenir.
3. **Detaylar:** "Detay Penceresini Aç" butonu ile karttan gelen ham veri ve ayrıntılı bilgiler ayrı bir pencerede incelenebilir.
4. **MIFARE İşlemleri:**
   - **Kimlik Doğrulama:** Anahtar tipi (Key A/B), anahtar numarası ve sektör numarası girilerek "Kimlik Doğrula" butonuna basılır. Sonuç ekranda gösterilir.
   - **Blok Okuma:** Blok numarası girilerek "Blok Oku" butonuna basılır. Okunan veri ekranda gösterilir.

## Ekran Görüntüsü ve Arayüz
Uygulama, kullanıcı dostu bir arayüze sahiptir. Port seçimi, kart bilgileri, MIFARE işlemleri ve hata mesajları kolayca takip edilebilir.

## Dosya ve Sınıf Yapısı
- **mainwindow.cpp/h/ui:** Ana pencere, port ve kart işlemleri, MIFARE fonksiyonları.
- **detailsdialog.cpp/h/ui:** Karttan gelen ham veri ve detayların gösterildiği pencere.
- **CardReaderApp.pro:** Qt proje yapılandırma dosyası.

## Geliştirici Bilgisi
- **Geliştirici:** İlhan Uzunoğlu
- **E-posta:** ilhanuzunoglu02@gmail.com
- **Staj Projesi:** Bu uygulama, 2025 yaz stajımda geliştirdiğim ikinci projedir ve kart okuyucu cihazları ile güvenli iletişim ve veri okuma üzerine odaklanmıştır.

## Katkı ve Lisans
Bu proje eğitim ve örnek amaçlıdır. Katkıda bulunmak isteyenler için açıktır.

---

Daha fazla bilgi veya katkı için iletişime geçebilirsiniz. 
