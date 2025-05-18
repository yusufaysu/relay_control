# ESP32 Akıllı Ev Kontrol Sistemi

Bu proje, **ESP32** tabanlı, Ethernet (ENC28J60) ile haberleşen, MCP23017 GPIO genişletici kullanan ve web tabanlı modern bir kontrol paneli sunan bir akıllı ev otomasyon sistemidir. Sistem, 32 giriş ve 32 çıkışa kadar genişleyebilir, kullanıcı dostu arayüzüyle aydınlatma ve panjur gruplarını kolayca yönetmenizi sağlar.

## Özellikler

- **Ethernet (ENC28J60) ile hızlı ve stabil bağlantı**
- **MCP23017 GPIO genişletici** ile 32 giriş ve 32 çıkış desteği
- **SPIFFS dosya sistemi** ile web arayüz dosyalarını ESP32 üzerinde barındırma
- **Modern, responsive ve dark-blue temalı web paneli**
- **Kullanıcı tanımlı grup ve çıkış yönetimi**
- **Panjur kontrolü için özel çıkış yönetimi**
- **Güvenli oturum yönetimi (cookie tabanlı login)**
- **JSON tabanlı API ile frontend-backend entegrasyonu**
- **cJSON ile güvenli ve hızlı veri işleme**
- **Stack overflow ve büyük JSON loglama için güvenlik önlemleri**
- **Kolay kurulum ve özelleştirme**

## Kullanılan Teknolojiler

- **ESP-IDF**: ESP32 geliştirme ortamı
- **C++**: Donanım ve backend kodları
- **HTML/CSS/JavaScript**: Modern web arayüzü (panel.html, login.html)
- **SPIFFS**: Web dosyalarının ESP32 üzerinde saklanması
- **cJSON**: JSON veri işleme
- **Ethernet (ENC28J60)**: Kablolu ağ bağlantısı
- **MCP23017**: I2C tabanlı GPIO genişletici

## Dosya Yapısı

```
├── main/
│   ├── main.cpp
│   └── inc/
│       ├── ethernet/
│       │   ├── ethernet.cpp / ethernet.hpp
│       ├── mcp23017/
│       │   ├── mcp23017.cpp / mcp23017.hpp
│       └── webserver/
│           ├── webserver.cpp / webserver.hpp
├── spiffs/
│   ├── panel.html
│   └── login.html
├── CMakeLists.txt
├── README.md
└── ...
```

## Kurulum

1. **ESP-IDF ortamını kurun**  
   [ESP-IDF Kurulum Rehberi](https://docs.espressif.com/projects/esp-idf/tr/latest/esp32/get-started/index.html)

2. **Bağımlılıkları kontrol edin**  
   - cJSON kütüphanesi projeye dahildir.
   - Gerekli ayarlar `CMakeLists.txt` ve `idf_component.yml` dosyalarında yapılmıştır.

3. **SPIFFS dosyalarını yükleyin**  
   `spiffs` klasöründeki `panel.html` ve `login.html` dosyalarını ESP32'ye yükleyin.

4. **Projeyi derleyin ve yükleyin**
   ```
   idf.py build
   idf.py -p [PORT] flash
   ```

5. **Cihazı başlatın ve web arayüzüne erişin**  
   Ethernet bağlantısı üzerinden cihazın IP adresini tarayıcıda açın.

## Web Panel Özellikleri

- **Giriş/Çıkış Grupları:**  
  Sınırsız sayıda aydınlatma ve panjur grubu ekleyebilir, isimlendirebilir ve yönetebilirsiniz.
- **Panjur Kontrolü:**  
  Her panjur grubu için iki çıkış (yukarı/aşağı) atanır ve isimleri değiştirilebilir.
- **Kullanıcı Dostu Arayüz:**  
  Tüm işlemler localStorage ile yönetilir, backend ile JSON tabanlı API üzerinden haberleşir.
- **Güvenli Giriş:**  
  login.html üzerinden oturum açmadan panele erişim engellenir.

## API Endpointleri

- `GET /api/outputs` — Çıkış durumlarını alır
- `GET /api/inputs` — Giriş durumlarını alır
- `POST /api/save_groups` — Grup yapılandırmasını kaydeder
- (Detaylı JSON formatı ve örnekler için kodu inceleyin.)

## Güvenlik ve Stabilite

- Oturum kontrolü cookie ile sağlanır.
- Büyük JSON loglama ve stack overflow için önlemler alınmıştır.
- Tüm girişler ve API istekleri doğrulanır.

## Katkı ve Lisans

Katkıda bulunmak için pull request gönderebilir veya issue açabilirsiniz.  
Lisans bilgisi için lütfen LICENSE dosyasını kontrol edin.
