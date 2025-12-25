#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

// batas dan konstanta
#define batasPelanggan 600
#define batasOrder 1800
#define batasBooking 1200
#define batasInventaris 3000

#define panjangString 128
#define panjangTampilan 512 // buffer tampilan besar untuk aman dari truncation
#define panjangRupiah 48    // buffer khusus format Rupiah

const char *daftarHari[7] = {
    "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu", "Minggu"
};

const int jamBuka = 8;
const int jamTutup = 18;

// struktur data
typedef struct {
    char namaPelanggan[panjangString];
    char nomorTelepon[panjangString];
    char modelMotor[panjangString];
    int kapasitasMesin;
    double saldo;
} DataPelanggan;

typedef struct {
    char namaItem[panjangString];
    char kategori[panjangString];
    char merekPlesetan[panjangString];
    double harga;
    int stok;
} DataInventaris;

typedef struct {
    char hari[panjangString];
    int jam;
    int terisi; // 0 tersedia, 1 terisi
    char namaPelanggan[panjangString];
    char nomorTelepon[panjangString];
} DataBooking;

typedef struct {
    char namaPelanggan[panjangString];
    char nomorTelepon[panjangString];
    char modelMotor[panjangString];
    int kapasitasMesin;
    char deskripsiOrder[256];
    double totalBiaya;
    char hariBooking[panjangString];
    int jamBooking;
} DataOrder;

// data global
DataPelanggan dataPelanggan[batasPelanggan];
int jumlahPelanggan = 0;

DataInventaris dataInventaris[batasInventaris];
int jumlahInventaris = 0;

DataBooking dataBooking[batasBooking];
int jumlahBooking = 0;

DataOrder dataOrder[batasOrder];
int jumlahOrder = 0;

const char *filePelanggan = "dataPelanggan.txt";
const char *fileOrder = "dataOrder.txt";
const char *fileBooking = "dataBooking.txt";
const char *fileInventaris = "dataInventaris.txt";

// util umum dan UI
void bersihkanLayar() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// bersihkan sisa input agar tidak “skip”
void kosongkanBaris() {
    int karakterMasuk;
    while ((karakterMasuk = getchar()) != '\n' && karakterMasuk != EOF) {}
}

void tekanEnter() {
    printf("\nTekan Enter untuk lanjut...");
    kosongkanBaris();
    getchar();
}

// format Rupiah dengan titik pemisah ribuan
void formatRupiah(double nilai, char *buffer, size_t ukuran) {
    long long bulat = (long long)(nilai + 0.5); // pembulatan
    char angka[64];
    snprintf(angka, sizeof(angka), "%lld", bulat);

    char hasil[panjangRupiah] = {0};
    int len = (int)strlen(angka);
    int hitung = 0;

    for (int i = len - 1; i >= 0; i--) {
        hasil[hitung++] = angka[i];
        if ((len - 1 - i) % 3 == 2 && i != 0) {
            hasil[hitung++] = '.';
        }
    }
    // balikkan hasil
    char akhir[panjangRupiah] = {0};
    int p = 0;
    for (int i = hitung - 1; i >= 0; i--) akhir[p++] = hasil[i];
    akhir[p] = '\0';

    snprintf(buffer, ukuran, "Rp %s", akhir);
}

// garis pemisah
void garisPemisah() {
    printf("=====================================================================\n");
}

void tampilJudulKotak(const char *judulTampil) {
    int lebar = 69;
    garisPemisah();
    int len = (int)strlen(judulTampil);
    int kiri = (lebar - len) / 2;
    if (kiri < 0) kiri = 0;
    printf("|");
    for (int i = 0; i < kiri; i++) printf(" ");
    printf("%s", judulTampil);
    int kanan = lebar - len - kiri;
    for (int i = 0; i < kanan; i++) printf(" ");
    printf("|\n");
    garisPemisah();
}

int cocokSubstring(const char *teksSumber, const char *teksCari) {
    char sumber[panjangString], cari[panjangString];
    snprintf(sumber, panjangString, "%s", teksSumber);
    snprintf(cari, panjangString, "%s", teksCari);
    for (int i = 0; sumber[i]; i++) sumber[i] = (char)tolower(sumber[i]);
    for (int i = 0; cari[i]; i++) cari[i] = (char)tolower(cari[i]);
    return strstr(sumber, cari) != NULL;
}


// WASD
int bacaTombol() {
#ifdef _WIN32
    int ch = getch();
    if (ch == 0 || ch == 224) {
        int ch2 = getch();
        return ch2;
    }
    return ch;
#else
    struct termios pengaturanAwal, pengaturanBaru;
    int karakterMasuk;
    tcgetattr(STDIN_FILENO, &pengaturanAwal);
    pengaturanBaru = pengaturanAwal;
    pengaturanBaru.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &pengaturanBaru);
    karakterMasuk = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &pengaturanAwal);
    return karakterMasuk;
#endif
}


// tampil menu dengan judul kotak dan petunjuk kontrol
void tampilMenuNavigasi(const char *judulTampil, const char **daftarOpsi, int jumlahOpsi, int posisiTerpilih, const char *petunjuk) {
    bersihkanLayar();
    tampilJudulKotak(judulTampil);
    for (int indeks = 0; indeks < jumlahOpsi; indeks++) {
        if (indeks == posisiTerpilih) printf("-> %s\n", daftarOpsi[indeks]);
        else printf("   %s\n", daftarOpsi[indeks]);
    }
    garisPemisah();
    printf("%s\n", petunjuk && strlen(petunjuk) ? petunjuk : "Kontrol: W/S atau A/D untuk naik/turun, Enter untuk pilih, Q untuk kembali.");
}


// loop navigasi WASD; return indeks pilihan atau -1 untuk kembali
int pilihDenganWASD(const char *judulTampil, const char **daftarOpsi, int jumlahOpsi) {
    int posisiTerpilih = 0;
    while (1) {
        tampilMenuNavigasi(judulTampil, daftarOpsi, jumlahOpsi, posisiTerpilih, NULL);
        int tombol = bacaTombol();
        if (tombol >= 'A' && tombol <= 'Z') tombol += ('a' - 'A');
        if (tombol == 'w' || tombol == 'a') {
            posisiTerpilih = (posisiTerpilih > 0) ? posisiTerpilih - 1 : jumlahOpsi - 1;
        } else if (tombol == 's' || tombol == 'd') {
            posisiTerpilih = (posisiTerpilih < jumlahOpsi - 1) ? posisiTerpilih + 1 : 0;
        } else if (tombol == '\r' || tombol == '\n') {
            return posisiTerpilih;
        } else if (tombol == 'q') {
            return -1;
        }
    }
}


// file IO
void simpanPelanggan() {
    FILE *fileData = fopen(filePelanggan, "w");
    if (!fileData) return;
    for (int i = 0; i < jumlahPelanggan; i++) {
        fprintf(fileData, "%s|%s|%s|%d|%.2f\n",
            dataPelanggan[i].namaPelanggan,
            dataPelanggan[i].nomorTelepon,
            dataPelanggan[i].modelMotor,
            dataPelanggan[i].kapasitasMesin,
            dataPelanggan[i].saldo
        );
    }
    fclose(fileData);
}

void muatPelanggan() {
    FILE *fileData = fopen(filePelanggan, "r");
    if (!fileData) return;
    jumlahPelanggan = 0;
    char barisData[512];
    while (fgets(barisData, sizeof(barisData), fileData)) {
        DataPelanggan pelangganBaru;
        char *fieldNama = strtok(barisData, "|");
        char *fieldTelepon = strtok(NULL, "|");
        char *fieldMotor = strtok(NULL, "|");
        char *fieldKapasitas = strtok(NULL, "|");
        char *fieldSaldo = strtok(NULL, "\n");
        if (!fieldNama || !fieldTelepon || !fieldMotor || !fieldKapasitas || !fieldSaldo) continue;
        snprintf(pelangganBaru.namaPelanggan, panjangString, "%s", fieldNama);
        snprintf(pelangganBaru.nomorTelepon, panjangString, "%s", fieldTelepon);
        snprintf(pelangganBaru.modelMotor, panjangString, "%s", fieldMotor);
        pelangganBaru.kapasitasMesin = atoi(fieldKapasitas);
        pelangganBaru.saldo = atof(fieldSaldo);
        if (jumlahPelanggan < batasPelanggan) dataPelanggan[jumlahPelanggan++] = pelangganBaru;
    }
    fclose(fileData);
}

void simpanOrder() {
    FILE *fileData = fopen(fileOrder, "w");
    if (!fileData) return;
    for (int i = 0; i < jumlahOrder; i++) {
        fprintf(fileData, "%s|%s|%s|%d|%s|%.2f|%s|%d\n",
            dataOrder[i].namaPelanggan,
            dataOrder[i].nomorTelepon,
            dataOrder[i].modelMotor,
            dataOrder[i].kapasitasMesin,
            dataOrder[i].deskripsiOrder,
            dataOrder[i].totalBiaya,
            dataOrder[i].hariBooking,
            dataOrder[i].jamBooking
        );
    }
    fclose(fileData);
}

void muatOrder() {
    FILE *fileData = fopen(fileOrder, "r");
    if (!fileData) return;
    jumlahOrder = 0;
    char barisData[1024];
    while (fgets(barisData, sizeof(barisData), fileData)) {
        DataOrder orderBaru;
        char *fieldNama = strtok(barisData, "|");
        char *fieldTelepon = strtok(NULL, "|");
        char *fieldMotor = strtok(NULL, "|");
        char *fieldKapasitas = strtok(NULL, "|");
        char *fieldDeskripsi = strtok(NULL, "|");
        char *fieldBiaya = strtok(NULL, "|");
        char *fieldHari = strtok(NULL, "|");
        char *fieldJam = strtok(NULL, "\n");
        if (!fieldNama || !fieldTelepon || !fieldMotor || !fieldKapasitas || !fieldDeskripsi || !fieldBiaya || !fieldHari || !fieldJam) continue;
        snprintf(orderBaru.namaPelanggan, panjangString, "%s", fieldNama);
        snprintf(orderBaru.nomorTelepon, panjangString, "%s", fieldTelepon);
        snprintf(orderBaru.modelMotor, panjangString, "%s", fieldMotor);
        orderBaru.kapasitasMesin = atoi(fieldKapasitas);
        snprintf(orderBaru.deskripsiOrder, 256, "%s", fieldDeskripsi);
        orderBaru.totalBiaya = atof(fieldBiaya);
        snprintf(orderBaru.hariBooking, panjangString, "%s", fieldHari);
        orderBaru.jamBooking = atoi(fieldJam);
        if (jumlahOrder < batasOrder) dataOrder[jumlahOrder++] = orderBaru;
    }
    fclose(fileData);
}

void simpanBooking() {
    FILE *fileData = fopen(fileBooking, "w");
    if (!fileData) return;
    for (int i = 0; i < jumlahBooking; i++) {
        fprintf(fileData, "%s|%d|%d|%s|%s\n",
            dataBooking[i].hari,
            dataBooking[i].jam,
            dataBooking[i].terisi,
            dataBooking[i].namaPelanggan,
            dataBooking[i].nomorTelepon
        );
    }
    fclose(fileData);
}

void muatBooking() {
    FILE *fileData = fopen(fileBooking, "r");
    if (!fileData) return;
    jumlahBooking = 0;
    char barisData[512];
    while (fgets(barisData, sizeof(barisData), fileData)) {
        DataBooking bookingBaru;
        char *fieldHari = strtok(barisData, "|");
        char *fieldJam = strtok(NULL, "|");
        char *fieldTerisi = strtok(NULL, "|");
        char *fieldNama = strtok(NULL, "|");
        char *fieldTelepon = strtok(NULL, "\n");
        if (!fieldHari || !fieldJam || !fieldTerisi || !fieldNama || !fieldTelepon) continue;
        snprintf(bookingBaru.hari, panjangString, "%s", fieldHari);
        bookingBaru.jam = atoi(fieldJam);
        bookingBaru.terisi = atoi(fieldTerisi);
        snprintf(bookingBaru.namaPelanggan, panjangString, "%s", fieldNama);
        snprintf(bookingBaru.nomorTelepon, panjangString, "%s", fieldTelepon);
        if (jumlahBooking < batasBooking) dataBooking[jumlahBooking++] = bookingBaru;
    }
    fclose(fileData);
}

void simpanInventaris() {
    FILE *fileData = fopen(fileInventaris, "w");
    if (!fileData) return;
    for (int i = 0; i < jumlahInventaris; i++) {
        fprintf(fileData, "%s|%s|%s|%.2f|%d\n",
            dataInventaris[i].namaItem,
            dataInventaris[i].kategori,
            dataInventaris[i].merekPlesetan,
            dataInventaris[i].harga,
            dataInventaris[i].stok
        );
    }
    fclose(fileData);
}

void muatInventaris() {
    FILE *fileData = fopen(fileInventaris, "r");
    if (!fileData) return;
    jumlahInventaris = 0;
    char barisData[512];
    while (fgets(barisData, sizeof(barisData), fileData)) {
        DataInventaris inventarisBaru;
        char *fieldNama = strtok(barisData, "|");
        char *fieldKategori = strtok(NULL, "|");
        char *fieldMerek = strtok(NULL, "|");
        char *fieldHarga = strtok(NULL, "|");
        char *fieldStok = strtok(NULL, "\n");
        if (!fieldNama || !fieldKategori || !fieldMerek || !fieldHarga || !fieldStok) continue;
        snprintf(inventarisBaru.namaItem, panjangString, "%s", fieldNama);
        snprintf(inventarisBaru.kategori, panjangString, "%s", fieldKategori);
        snprintf(inventarisBaru.merekPlesetan, panjangString, "%s", fieldMerek);
        inventarisBaru.harga = atof(fieldHarga);
        inventarisBaru.stok = atoi(fieldStok);
        if (jumlahInventaris < batasInventaris) dataInventaris[jumlahInventaris++] = inventarisBaru;
    }
    fclose(fileData);
}

const char *motorPopuler[] = {
    // Hondo (Honda)
    "Hondo Beat 110", "Hondo Scoopy 110", "Hondo Vario 125", "Hondo Vario 150", "Hondo Vario 160",
    "Hondo PCX 160", "Hondo ADV 160", "Hondo CB150R 150", "Hondo CBR150R 150", "Hondo Supra X 125",
    // Yamaho (Yamaha)
    "Yamaho NMAX 155", "Yamaho Aerox 155", "Yamaho Jupiter Z1 115", "Yamaho MX King 150", "Yamaho R15 155",
    // Suzeko (Suzuki)
    "Suzeko Satria F150 150", "Suzeko GSX-R150 150", "Suzeko GSX-S150 150", "Suzeko Spin 125", "Suzeko Nex II 115",
    // Kowoseke (Kawasaki)
    "Kowoseke Ninja 250 250", "Kowoseke Z250 250", "Kowoseke KLX 150 150", "Kowoseke W175 175",
    // KTMo (KTM), Bonollo (Benelli), Bomwu (BMW)
    "KTMo Duke 200 200", "KTMo Duke 250 250", "KTMo RC 390 390",
    "Bonollo TNT 250 250", "Bomwu C 400 GT 400"
};
const int jumlahMotorPopuler = sizeof(motorPopuler) / sizeof(motorPopuler[0]);


// inventaris awal
void tambahInventaris() {
    DataInventaris daftarInventarisAwal[] = {
        // oli
        {"Oli Mineral 10W-40", "Oli", "Shullo Advance", 65000, 120},
        {"Oli Semi Synthetic 10W-40", "Oli", "Motuol Racing", 95000, 100},
        {"Oli Fully Synthetic 5W-30", "Oli", "Costrul Power", 170000, 75},
        {"Oli Fully Synthetic 10W-50", "Oli", "Endoranz Oil", 150000, 85},
        {"Oli Ester 0W-30", "Oli", "Pertanimo Turbo", 210000, 55},

        // suspensi
        {"Shock Belakang Tabung A", "Suspensi", "Showo", 480000, 40},
        {"Shock Depan Cartridge B", "Suspensi", "Koyobo", 820000, 30},
        {"Fork Upgrade C", "Suspensi", "YSS", 1250000, 25},
        {"Shock Belakang Adjustable D", "Suspensi", "RCB", 920000, 35},
        {"Fork USD E", "Suspensi", "TDRo Forks", 1850000, 15},

        // rem
        {"Kaliper Rem Dua Piston", "Rem", "Nessen", 550000, 60},
        {"Kaliper Rem Empat Piston", "Rem", "Brimbo", 1350000, 25},
        {"Master Rem Adjustable", "Rem", "RPDush Brakes", 620000, 50},
        {"Disc Brake Wave Pattern", "Rem", "Tukicu", 380000, 70},
        {"Selang Rem Steel Braided", "Rem", "Okeshi Brake", 290000, 80},

        // ban
        {"Ban Depan Sport 90/80-14", "Ban", "FDR", 320000, 120},
        {"Ban Belakang Sport 120/70-14", "Ban", "IRC", 420000, 100},
        {"Ban Touring 100/80-17", "Ban", "Michelen", 520000, 90},
        {"Ban Harian 80/90-14", "Ban", "Bridgestune", 280000, 140},
        {"Ban Performance 140/70-17", "Ban", "Dunlup", 590000, 70},

        // velg
        {"Velg Racing 14 Inch", "Velg", "RCB Wheels", 820000, 35},
        {"Velg Racing 17 Inch", "Velg", "TDRo Racing", 1090000, 28},
        {"Velg Supermoto 17 Inch", "Velg", "Comit", 1290000, 22},
        {"Velg Lightweight 17 Inch", "Velg", "Enkii", 1450000, 18},
        {"Velg Forged 17 Inch", "Velg", "UZ Racing", 2650000, 12},

        // knalpot
        {"Knalpot Full System Stainless", "Knalpot", "Akrapovuc", 3250000, 12},
        {"Knalpot Slip-On Carbon", "Knalpot", "Yoshimuro", 1750000, 20},
        {"Knalpot Slip-On Titanium", "Knalpot", "SCProjecu", 2350000, 15},
        {"Knalpot Racing Aluminized", "Knalpot", "Arruw", 1550000, 25},
        {"Knalpot Harian Silent", "Knalpot", "ProSpuud", 950000, 35},

        // boreUpPaket
        {"Paket Bore Up 115cc", "BoreUpPaket", "TDRo Power", 950000, 20},
        {"Paket Bore Up 125cc", "BoreUpPaket", "Kowohoro Racing", 1200000, 18},
        {"Paket Bore Up 150cc", "BoreUpPaket", "UMO Performance", 1750000, 15},
        {"Paket Bore Up 180cc", "BoreUpPaket", "Kutacu", 2350000, 10},
        {"Paket Bore Up 200cc", "BoreUpPaket", "BRT Engine", 2950000, 8},

        // boreUpKomponen
        {"Piston Kit High Compression", "BoreUpKomponen", "TDRo Piston", 480000, 40},
        {"Camshaft Racing Stage 1", "BoreUpKomponen", "Kowohoro Cams", 520000, 35},
        {"Karburator 28mm", "BoreUpKomponen", "Yoshimuro Carb", 750000, 20},
        {"Injector High Flow", "BoreUpKomponen", "UMO Fuel", 890000, 18},
        {"Gasket Set Performance", "BoreUpKomponen", "Nessen Seal", 210000, 60},

        // elektrikal
        {"CDI Racing Programmable", "Elektrikal", "Rextur CDI", 750000, 25},
        {"Coil High Voltage", "Elektrikal", "BRT Ignitoge", 420000, 40},
        {"Speedometer Digital Multifungsi", "Elektrikal", "Kusu Digital", 980000, 15},
        {"Saklar Set Racing", "Elektrikal", "Rizomu Switch", 320000, 50},
        {"Regulator Stabilizer", "Elektrikal", "Doytono", 280000, 45},

        // filter
        {"Filter Udara Racing", "Filter", "Ferrux", 650000, 25},
        {"Filter Udara Panel", "Filter", "KNN Air", 480000, 35},
        {"Filter Udara Cone", "Filter", "HKS", 520000, 30},
        {"Filter Udara High Flow", "Filter", "Simotu", 530000, 28},
        {"Filter Oli Performance", "Filter", "Pupircross", 270000, 60},

        // servis
        {"Servis Rutin Ringan", "Servis", "YONKJAWA", 80000, 9999},
        {"Servis Rutin Standar", "Servis", "YONKJAWA", 120000, 9999},
        {"Servis Rutin Pro", "Servis", "YONKJAWA", 180000, 9999},
        {"Tune Up Lengkap", "Servis", "YONKJAWA", 250000, 9999},
        {"Setel Klep dan Throttle Body", "Servis", "YONKJAWA", 220000, 9999},

        // lampu
        {"Headlamp LED Putih", "Lampu", "Phelups", 250000, 50},
        {"Lampu Sein Kuning", "Lampu", "Osrum", 75000, 100},
        {"Lampu Rem LED Merah", "Lampu", "Busch", 120000, 80},
        {"Lampu Hazard Universal", "Lampu", "Stonliy", 95000, 60},
        {"Lampu Kabut Motor", "Lampu", "Norvu", 180000, 40},

        // rantai dan gir
        {"Rantai Racing Gold", "Rantai", "DID", 350000, 30},
        {"Gear Depan 14T", "Rantai", "Sprucket", 120000, 50},
        {"Gear Belakang 42T", "Rantai", "Sunstor", 150000, 40},
        {"Set Rantai + Sprocket", "Rantai", "RK", 480000, 25},
        {"Rantai Heavy Duty", "Rantai", "EK", 400000, 20},

        // bodyKit
        {"Spion Racing Carbon", "BodyKit", "Rizomu", 320000, 60},
        {"Handgrip Karet Racing", "BodyKit", "Progrup", 150000, 80},
        {"Cover Engine Protector", "BodyKit", "KusuX", 280000, 40},
        {"Windshield Sport", "BodyKit", "Puog", 450000, 30},
        {"Footstep Racing Adjustable", "BodyKit", "Lughtech", 520000, 25},

        // pendingin
        {"Radiator Racing Aluminium", "Pendingin", "Kowohoro", 1350000, 10},
        {"Selang Radiator Silicone", "Pendingin", "Somcu", 480000, 25},
        {"Kipas Pendingin Motor", "Pendingin", "Spal", 320000, 30},
        {"Coolant Racing 1L", "Pendingin", "Motuol Cool", 95000, 100},
        {"Oil Cooler Kit", "Pendingin", "TDRo", 1850000, 12}
    };

    int jumlahAwalInventaris = (int)(sizeof(daftarInventarisAwal) / sizeof(daftarInventarisAwal[0]));
    for (int i = 0; i < jumlahAwalInventaris; i++) {
        if (jumlahInventaris < batasInventaris) dataInventaris[jumlahInventaris++] = daftarInventarisAwal[i];
    }
}


// sorting & searching
void urutInventarisNaik() {
    for (int i = 0; i < jumlahInventaris - 1; i++) {
        for (int j = 0; j < jumlahInventaris - i - 1; j++) {
            if (dataInventaris[j].harga > dataInventaris[j+1].harga) {
                DataInventaris dataSementara = dataInventaris[j];
                dataInventaris[j] = dataInventaris[j+1];
                dataInventaris[j+1] = dataSementara;
            }
        }
    }
}

void urutInventarisTurun() {
    for (int i = 0; i < jumlahInventaris - 1; i++) {
        int posisiMaksimum = i;
        for (int j = i + 1; j < jumlahInventaris; j++) {
            if (dataInventaris[j].harga > dataInventaris[posisiMaksimum].harga) posisiMaksimum = j;
        }
        if (posisiMaksimum != i) {
            DataInventaris dataSementara = dataInventaris[i];
            dataInventaris[i] = dataInventaris[posisiMaksimum];
            dataInventaris[posisiMaksimum] = dataSementara;
        }
    }
}

int cariKategori(const char *namaKategori, int *hasilIndeks, int batasHasil) {
    int jumlahHasil = 0;
    for (int i = 0; i < jumlahInventaris && jumlahHasil < batasHasil; i++) {
        if (strcmp(dataInventaris[i].kategori, namaKategori) == 0) {
            hasilIndeks[jumlahHasil++] = i;
        }
    }
    return jumlahHasil;
}


// booking
int cariSlot(const char *namaHari, int jamPilihan) {
    for (int i = 0; i < jumlahBooking; i++) {
        if (strcmp(dataBooking[i].hari, namaHari) == 0 && dataBooking[i].jam == jamPilihan) return i;
    }
    return -1;
}

void buatSlotMinggu() {
    for (int indeksHari = 0; indeksHari < 7; indeksHari++) {
        const char *namaHari = daftarHari[indeksHari];
        if (strcmp(namaHari, "Minggu") == 0) continue;
        for (int jamIterasi = jamBuka; jamIterasi <= jamTutup; jamIterasi++) {
            if (jumlahBooking >= batasBooking) break;
            if (cariSlot(namaHari, jamIterasi) >= 0) continue;
            DataBooking bookingBaru;
            snprintf(bookingBaru.hari, panjangString, "%s", namaHari);
            bookingBaru.jam = jamIterasi;
            bookingBaru.terisi = (jamIterasi == 10 || jamIterasi == 11 || jamIterasi == 15) ? 1 : 0; /* COMMAND: jam ramai otomatis terisi */
            if (bookingBaru.terisi) {
                snprintf(bookingBaru.namaPelanggan, panjangString, "%s", "Terisi Otomatis");
                snprintf(bookingBaru.nomorTelepon, panjangString, "%s", "000000");
            } else {
                bookingBaru.namaPelanggan[0] = 0;
                bookingBaru.nomorTelepon[0] = 0;
            }
            dataBooking[jumlahBooking++] = bookingBaru;
        }
    }
    simpanBooking();
}

void tampilSlotHari(const char *namaHari) {
    tampilJudulKotak("Slot Booking");
    printf("Hari %s (08:00–18:00)\n", namaHari);
    garisPemisah();
    for (int jamIterasi = jamBuka; jamIterasi <= jamTutup; jamIterasi++) {
        int posisiSlot = cariSlot(namaHari, jamIterasi);
        if (posisiSlot >= 0) {
            printf(" %02d:00 - %s", jamIterasi, dataBooking[posisiSlot].terisi ? "Terisi" : "Tersedia");
            if (dataBooking[posisiSlot].terisi && strlen(dataBooking[posisiSlot].namaPelanggan) > 0) {
                printf(" | %s (%s)", dataBooking[posisiSlot].namaPelanggan, dataBooking[posisiSlot].nomorTelepon);
            }
            printf("\n");
        }
    }
}

int pilihJamTersedia(const char *namaHari) {
    int daftarJamTersedia[16];
    int jumlahJamTersedia = 0;
    for (int jamIterasi = jamBuka; jamIterasi <= jamTutup; jamIterasi++) {
        int posisiSlot = cariSlot(namaHari, jamIterasi);
        if (posisiSlot >= 0 && dataBooking[posisiSlot].terisi == 0) {
            daftarJamTersedia[jumlahJamTersedia++] = jamIterasi;
        }
    }
    if (jumlahJamTersedia == 0) return -1;

    const char *opsiJam[16];
    char bufferJam[16][panjangString];
    for (int i = 0; i < jumlahJamTersedia; i++) {
        snprintf(bufferJam[i], panjangString, "%02d:00", daftarJamTersedia[i]);
        opsiJam[i] = bufferJam[i];
    }
    int pilihan = pilihDenganWASD("Pilih jam tersedia", opsiJam, jumlahJamTersedia);
    if (pilihan < 0) return -1;
    return daftarJamTersedia[pilihan];
}

int pesanSlot(const char *namaHari, int jamPilihan, const char *namaPelanggan, const char *nomorTelepon) {
    int posisiSlot = cariSlot(namaHari, jamPilihan);
    if (posisiSlot < 0 || dataBooking[posisiSlot].terisi) return 0;
    dataBooking[posisiSlot].terisi = 1;
    snprintf(dataBooking[posisiSlot].namaPelanggan, panjangString, "%s", namaPelanggan);
    snprintf(dataBooking[posisiSlot].nomorTelepon, panjangString, "%s", nomorTelepon);
    simpanBooking();
    return 1;
}


// pilih inventaris (WASD, aman truncation)
int pilihDariKategori(const char *namaKategori, int *indeksTerpilih, int batasPilihan) {
    int indeksKategoriSumber[batasInventaris];
    int jumlahKategori = cariKategori(namaKategori, indeksKategoriSumber, batasInventaris);
    if (jumlahKategori == 0) {
        printf("Tidak ada item pada kategori %s.\n", namaKategori);
        tekanEnter();
        return 0;
    }

    const char *opsiNamaItem[batasInventaris];
    char bufferItem[batasInventaris][panjangTampilan];

    for (int i = 0; i < jumlahKategori; i++) {
        int indeksItem = indeksKategoriSumber[i];
        char hargaRupiah[panjangRupiah];
        formatRupiah(dataInventaris[indeksItem].harga, hargaRupiah, sizeof(hargaRupiah));

        char namaPendek[panjangString], merekPendek[panjangString];
        snprintf(namaPendek, sizeof(namaPendek), "%.127s", dataInventaris[indeksItem].namaItem);
        snprintf(merekPendek, sizeof(merekPendek), "%.127s", dataInventaris[indeksItem].merekPlesetan);

        snprintf(bufferItem[i], sizeof(bufferItem[i]), "%s | %s | %s | Stok: %d",
                 namaPendek, merekPendek, hargaRupiah, dataInventaris[indeksItem].stok);
        opsiNamaItem[i] = bufferItem[i];
    }

    int jumlahTerpilih = 0;
    while (jumlahTerpilih < batasPilihan) {
        int pilihan = pilihDenganWASD(namaKategori, opsiNamaItem, jumlahKategori);
        if (pilihan < 0) break;
        int indeksItem = indeksKategoriSumber[pilihan];
        indeksTerpilih[jumlahTerpilih++] = indeksItem;
        printf("Ditambahkan: %s\n", dataInventaris[indeksItem].namaItem);
        garisPemisah();
        printf("Tambah lagi? (Enter untuk lanjut memilih, Q untuk selesai)\n");
        int tombol = bacaTombol();
        if (tombol == 'q' || tombol == 'Q') break;
    }
    return jumlahTerpilih;
}


// checkout
int lakukanCheckout(const char *namaPelanggan, const char *nomorTelepon,
                    const char *modelMotor, int kapasitasMesin,
                    const char *namaHari, int jamBooking,
                    int *indeksItemTerpilih, int jumlahItemTerpilih,
                    const char *deskripsiTambahan) {
    double totalBiaya = 0.0;
    for (int i = 0; i < jumlahItemTerpilih; i++) {
        totalBiaya += dataInventaris[indeksItemTerpilih[i]].harga;
    }

    bersihkanLayar();
    tampilJudulKotak("Ringkasan Order");
    char totalRupiah[panjangRupiah];
    formatRupiah(totalBiaya, totalRupiah, sizeof(totalRupiah));

    printf("Pelanggan: %s (%s)\n", namaPelanggan, nomorTelepon);
    printf("Motor: %s (%dcc)\n", modelMotor, kapasitasMesin);
    printf("Jadwal: %s %02d:00\n", namaHari, jamBooking);
    garisPemisah();
    printf("Item:\n");
    for (int i = 0; i < jumlahItemTerpilih; i++) {
        char hargaRupiah[panjangRupiah];
        formatRupiah(dataInventaris[indeksItemTerpilih[i]].harga, hargaRupiah, sizeof(hargaRupiah));
        printf(" - %s | %s | %s\n",
               dataInventaris[indeksItemTerpilih[i]].namaItem,
               dataInventaris[indeksItemTerpilih[i]].merekPlesetan,
               hargaRupiah);
    }
    if (deskripsiTambahan && strlen(deskripsiTambahan) > 0) {
        garisPemisah();
        printf("Deskripsi: %s\n", deskripsiTambahan);
    }
    garisPemisah();
    printf("Total biaya: %s\n", totalRupiah);
    garisPemisah();
    printf("Tekan Enter untuk konfirmasi, Q untuk batalkan.\n");
    int tombol = bacaTombol();
    if (tombol == 'q' || tombol == 'Q') return 0;

    int posisiPelanggan = -1;
    for (int i = 0; i < jumlahPelanggan; i++) {
        if (strcmp(dataPelanggan[i].nomorTelepon, nomorTelepon) == 0) { posisiPelanggan = i; break; }
    }
    if (posisiPelanggan < 0) {
        printf("Pelanggan tidak ditemukan.\n");
        tekanEnter();
        return 0;
    }
    if (dataPelanggan[posisiPelanggan].saldo < totalBiaya) {
        char saldoRupiah[panjangRupiah];
        formatRupiah(dataPelanggan[posisiPelanggan].saldo, saldoRupiah, sizeof(saldoRupiah));
        printf("Saldo tidak cukup. Saldo: %s\n", saldoRupiah);
        printf("Top up? (Enter untuk top up, Q untuk batal)\n");
        int tombolTopUp = bacaTombol();
        if (tombolTopUp == 'q' || tombolTopUp == 'Q') {
            printf("Pembayaran dibatalkan.\n");
            tekanEnter();
            return 0;
        }
        printf("Masukkan jumlah top up: ");
        double jumlahTopUpMasuk;
        scanf("%lf", &jumlahTopUpMasuk);
        kosongkanBaris();
        if (jumlahTopUpMasuk > 0) {
            dataPelanggan[posisiPelanggan].saldo += jumlahTopUpMasuk;
            simpanPelanggan();
            char saldoBaru[panjangRupiah];
            formatRupiah(dataPelanggan[posisiPelanggan].saldo, saldoBaru, sizeof(saldoBaru));
            printf("Saldo baru: %s\n", saldoBaru);
        }
        if (dataPelanggan[posisiPelanggan].saldo < totalBiaya) {
            printf("Saldo tetap kurang. Gagal.\n");
            tekanEnter();
            return 0;
        }
    }

    dataPelanggan[posisiPelanggan].saldo -= totalBiaya;
    simpanPelanggan();

    for (int i = 0; i < jumlahItemTerpilih; i++) {
        int indeksItem = indeksItemTerpilih[i];
        if (dataInventaris[indeksItem].stok > 0) dataInventaris[indeksItem].stok--;
    }
    simpanInventaris();

    if (jumlahOrder < batasOrder) {
        DataOrder orderBaru;
        snprintf(orderBaru.namaPelanggan, panjangString, "%s", namaPelanggan);
        snprintf(orderBaru.nomorTelepon, panjangString, "%s", nomorTelepon);
        snprintf(orderBaru.modelMotor, panjangString, "%s", modelMotor);
        orderBaru.kapasitasMesin = kapasitasMesin;
        char ringkasanItem[256] = {0};
        for (int i = 0; i < jumlahItemTerpilih; i++) {
            strcat(ringkasanItem, dataInventaris[indeksItemTerpilih[i]].namaItem);
            if (i < jumlahItemTerpilih - 1) strcat(ringkasanItem, ", ");
        }
        if (deskripsiTambahan && strlen(deskripsiTambahan) > 0) {
            strcat(ringkasanItem, " | ");
            strcat(ringkasanItem, deskripsiTambahan);
        }
        snprintf(orderBaru.deskripsiOrder, 256, "%s", ringkasanItem);
        orderBaru.totalBiaya = totalBiaya;
        snprintf(orderBaru.hariBooking, panjangString, "%s", namaHari);
        orderBaru.jamBooking = jamBooking;
        dataOrder[jumlahOrder++] = orderBaru;
        simpanOrder();
    }

    pesanSlot(namaHari, jamBooking, namaPelanggan, nomorTelepon);

    char sisaSaldoRupiah[panjangRupiah];
    formatRupiah(dataPelanggan[posisiPelanggan].saldo, sisaSaldoRupiah, sizeof(sisaSaldoRupiah));
    printf("\nPembayaran berhasil. Sisa saldo: %s\n", sisaSaldoRupiah);
    tekanEnter();
    return 1;
}


// layanan
void layananServis(const DataPelanggan *pelangganTerpilih) {
    int indeksItemTerpilih[10];
    int jumlahItemTerpilih = pilihDariKategori("Servis", indeksItemTerpilih, 3);
    if (jumlahItemTerpilih == 0) return;

    const char *opsiHari[] = { "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
    int pilihanHari = pilihDenganWASD("Pilih hari servis", opsiHari, 6);
    if (pilihanHari < 0) return;
    const char *namaHariTerpilih = opsiHari[pilihanHari];

    int jamTerpilih = pilihJamTersedia(namaHariTerpilih);
    if (jamTerpilih < 0) { printf("Tidak ada slot.\n"); tekanEnter(); return; }

    lakukanCheckout(pelangganTerpilih->namaPelanggan, pelangganTerpilih->nomorTelepon, pelangganTerpilih->modelMotor, pelangganTerpilih->kapasitasMesin, namaHariTerpilih, jamTerpilih, indeksItemTerpilih, jumlahItemTerpilih, "Servis rutin");
}

void layananOli(const DataPelanggan *pelangganTerpilih) {
    int indeksItemTerpilih[10];
    int jumlahItemTerpilih = pilihDariKategori("Oli", indeksItemTerpilih, 3);
    if (jumlahItemTerpilih == 0) return;

    const char *opsiHari[] = { "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
    int pilihanHari = pilihDenganWASD("Pilih hari ganti oli", opsiHari, 6);
    if (pilihanHari < 0) return;
    const char *namaHariTerpilih = opsiHari[pilihanHari];

    int jamTerpilih = pilihJamTersedia(namaHariTerpilih);
    if (jamTerpilih < 0) { printf("Tidak ada slot.\n"); tekanEnter(); return; }

    lakukanCheckout(pelangganTerpilih->namaPelanggan, pelangganTerpilih->nomorTelepon, pelangganTerpilih->modelMotor, pelangganTerpilih->kapasitasMesin, namaHariTerpilih, jamTerpilih, indeksItemTerpilih, jumlahItemTerpilih, "Ganti oli");
}

void layananBoreUp(const DataPelanggan *pelangganTerpilih) {
    const char *opsiJenis[] = { "Paket bore up", "Komponen bore up", "Kembali" };
    int pilihanJenis = pilihDenganWASD("Pilih layanan bore up", opsiJenis, 3);
    if (pilihanJenis < 0 || pilihanJenis == 2) return;

    int indeksItemTerpilih[20];
    int jumlahItemTerpilih = (pilihanJenis == 0) ?
        pilihDariKategori("BoreUpPaket", indeksItemTerpilih, 5) :
        pilihDariKategori("BoreUpKomponen", indeksItemTerpilih, 10);
    if (jumlahItemTerpilih == 0) return;

    const char *opsiHari[] = { "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
    int pilihanHari = pilihDenganWASD("Pilih hari bore up", opsiHari, 6);
    if (pilihanHari < 0) return;
    const char *namaHariTerpilih = opsiHari[pilihanHari];

    int jamTerpilih = pilihJamTersedia(namaHariTerpilih);
    if (jamTerpilih < 0) { printf("Tidak ada slot.\n"); tekanEnter(); return; }

    const char *desk = (pilihanJenis == 0) ? "Paket bore up" : "Komponen bore up";
    lakukanCheckout(pelangganTerpilih->namaPelanggan, pelangganTerpilih->nomorTelepon, pelangganTerpilih->modelMotor, pelangganTerpilih->kapasitasMesin, namaHariTerpilih, jamTerpilih, indeksItemTerpilih, jumlahItemTerpilih, desk);
}

void layananKakiKaki(const DataPelanggan *pelangganTerpilih) {
    const char *opsiKategori[] = { "Suspensi", "Rem", "Ban", "Velg", "Knalpot", "Rantai", "BodyKit", "Kembali" };
    int pilihanKategori = pilihDenganWASD("Pilih kategori upgrade kaki-kaki", opsiKategori, 8);
    if (pilihanKategori < 0 || pilihanKategori == 7) return;

    const char *namaKategoriTerpilih = opsiKategori[pilihanKategori];
    int indeksItemTerpilih[20];
    int jumlahItemTerpilih = pilihDariKategori(namaKategoriTerpilih, indeksItemTerpilih, 10);
    if (jumlahItemTerpilih == 0) return;

    const char *opsiHari[] = { "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
    int pilihanHari = pilihDenganWASD("Pilih hari upgrade", opsiHari, 6);
    if (pilihanHari < 0) return;
    const char *namaHariTerpilih = opsiHari[pilihanHari];

    int jamTerpilih = pilihJamTersedia(namaHariTerpilih);
    if (jamTerpilih < 0) { printf("Tidak ada slot.\n"); tekanEnter(); return; }

    char deskripsiLayanan[64];
    snprintf(deskripsiLayanan, 64, "Upgrade kaki-kaki (%s)", namaKategoriTerpilih);
    lakukanCheckout(pelangganTerpilih->namaPelanggan, pelangganTerpilih->nomorTelepon, pelangganTerpilih->modelMotor, pelangganTerpilih->kapasitasMesin, namaHariTerpilih, jamTerpilih, indeksItemTerpilih, jumlahItemTerpilih, deskripsiLayanan);
}

void layananCekRutin(const DataPelanggan *pelangganTerpilih) {
    const char *opsiKategori[] = { "Rem", "Oli", "Filter", "Elektrikal", "Pendingin", "Kembali" };
    int pilihanKategori = pilihDenganWASD("Pilih kategori cek rutin", opsiKategori, 6);
    if (pilihanKategori < 0 || pilihanKategori == 5) return;

    const char *namaKategoriTerpilih = opsiKategori[pilihanKategori];
    int indeksItemTerpilih[10];
    int jumlahItemTerpilih = pilihDariKategori(namaKategoriTerpilih, indeksItemTerpilih, 5);
    if (jumlahItemTerpilih == 0) return;

    const char *opsiHari[] = { "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
    int pilihanHari = pilihDenganWASD("Pilih hari cek rutin", opsiHari, 6);
    if (pilihanHari < 0) return;
    const char *namaHariTerpilih = opsiHari[pilihanHari];

    int jamTerpilih = pilihJamTersedia(namaHariTerpilih);
    if (jamTerpilih < 0) { printf("Tidak ada slot.\n"); tekanEnter(); return; }

    char deskripsiLayanan[64];
    snprintf(deskripsiLayanan, 64, "Cek rutin (%s)", namaKategoriTerpilih);
    lakukanCheckout(pelangganTerpilih->namaPelanggan, pelangganTerpilih->nomorTelepon, pelangganTerpilih->modelMotor, pelangganTerpilih->kapasitasMesin, namaHariTerpilih, jamTerpilih, indeksItemTerpilih, jumlahItemTerpilih, deskripsiLayanan);
}


// elanggan
int cariPelangganTelepon(const char *nomorTelepon) {
    for (int i = 0; i < jumlahPelanggan; i++) {
        if (strcmp(dataPelanggan[i].nomorTelepon, nomorTelepon) == 0) return i;
    }
    return -1;
}

void tampilPelangganRingkas() {
    bersihkanLayar();
    tampilJudulKotak("Daftar Pelanggan");
    for (int i = 0; i < jumlahPelanggan; i++) {
        char saldoRupiah[panjangRupiah];
        formatRupiah(dataPelanggan[i].saldo, saldoRupiah, sizeof(saldoRupiah));
        printf("%d. %s | %s | %s | %dcc | Saldo: %s\n",
            i + 1,
            dataPelanggan[i].namaPelanggan,
            dataPelanggan[i].nomorTelepon,
            dataPelanggan[i].modelMotor,
            dataPelanggan[i].kapasitasMesin,
            saldoRupiah
        );
    }
    garisPemisah();
    tekanEnter();
}

void tambahPelanggan() {
    bersihkanLayar();
    tampilJudulKotak("Tambah Pelanggan");
    DataPelanggan pelangganBaru;

    printf("Masukkan nama pelanggan: ");
    fgets(pelangganBaru.namaPelanggan, panjangString, stdin);
    pelangganBaru.namaPelanggan[strcspn(pelangganBaru.namaPelanggan, "\n")] = 0;

    printf("Masukkan nomor telepon: ");
    fgets(pelangganBaru.nomorTelepon, panjangString, stdin);
    pelangganBaru.nomorTelepon[strcspn(pelangganBaru.nomorTelepon, "\n")] = 0;

    printf("Masukkan model motor: ");
    fgets(pelangganBaru.modelMotor, panjangString, stdin);
    pelangganBaru.modelMotor[strcspn(pelangganBaru.modelMotor, "\n")] = 0;

    printf("Masukkan kapasitas mesin (1–499): ");
    int kapasitasMasuk;
    scanf("%d", &kapasitasMasuk);
    kosongkanBaris();
    pelangganBaru.kapasitasMesin = kapasitasMasuk;
    if (pelangganBaru.kapasitasMesin <= 0 || pelangganBaru.kapasitasMesin >= 500) {
        printf("Kapasitas tidak valid.\n");
        tekanEnter();
        return;
    }

    printf("Masukkan saldo awal: ");
    double saldoMasuk;
    scanf("%lf", &saldoMasuk);
    kosongkanBaris();
    pelangganBaru.saldo = saldoMasuk;

    if (jumlahPelanggan < batasPelanggan) {
        dataPelanggan[jumlahPelanggan++] = pelangganBaru;
        simpanPelanggan();
        printf("\nPelanggan ditambahkan.\n");
    } else {
        printf("\nGagal: kapasitas penuh.\n");
    }
    tekanEnter();
}

void tambahSaldoPelanggan() {
    bersihkanLayar();
    tampilJudulKotak("Top Up Saldo");
    char nomorTelepon[panjangString];
    printf("Masukkan nomor telepon: ");
    fgets(nomorTelepon, panjangString, stdin);
    nomorTelepon[strcspn(nomorTelepon, "\n")] = 0;

    int posisiPelanggan = cariPelangganTelepon(nomorTelepon);
    if (posisiPelanggan < 0) {
        printf("Pelanggan tidak ditemukan.\n");
        tekanEnter();
        return;
    }
    char saldoRupiah[panjangRupiah];
    formatRupiah(dataPelanggan[posisiPelanggan].saldo, saldoRupiah, sizeof(saldoRupiah));
    printf("Saldo saat ini: %s\n", saldoRupiah);
    printf("Masukkan jumlah top up: ");
    double jumlahTopUp;
    scanf("%lf", &jumlahTopUp);
    kosongkanBaris();
    if (jumlahTopUp <= 0) {
        printf("Jumlah tidak valid.\n");
        tekanEnter();
        return;
    }
    dataPelanggan[posisiPelanggan].saldo += jumlahTopUp;
    simpanPelanggan();
    char saldoBaru[panjangRupiah];
    formatRupiah(dataPelanggan[posisiPelanggan].saldo, saldoBaru, sizeof(saldoBaru));
    printf("Saldo baru: %s\n", saldoBaru);
    tekanEnter();
}


// menu pelanggan terpilih (WASD)
void menuLayananPelanggan(int posisiPelanggan) {
    DataPelanggan *pelangganTerpilih = &dataPelanggan[posisiPelanggan];
    const char *opsiLayanan[] = {
        "Servis rutin",
        "Ganti oli",
        "Bore up mesin",
        "Upgrade kaki-kaki",
        "Cek rutin",
        "Top up saldo",
        "Kembali"
    };
    int jumlahOpsi = (int)(sizeof(opsiLayanan) / sizeof(opsiLayanan[0]));

    while (1) {
        char judulTampil[panjangTampilan];
        char saldoRupiah[panjangRupiah];
        formatRupiah(pelangganTerpilih->saldo, saldoRupiah, sizeof(saldoRupiah));
        snprintf(judulTampil, sizeof(judulTampil),
                 "Menu Layanan Pelanggan\n%s (%s)\nMotor: %s (%dcc)\nSaldo: %s",
                 pelangganTerpilih->namaPelanggan,
                 pelangganTerpilih->nomorTelepon,
                 pelangganTerpilih->modelMotor,
                 pelangganTerpilih->kapasitasMesin,
                 saldoRupiah);

        int pilihan = pilihDenganWASD(judulTampil, opsiLayanan, jumlahOpsi);
        if (pilihan < 0 || pilihan == 6) break;
        if (pilihan == 0) layananServis(pelangganTerpilih);
        else if (pilihan == 1) layananOli(pelangganTerpilih);
        else if (pilihan == 2) layananBoreUp(pelangganTerpilih);
        else if (pilihan == 3) layananKakiKaki(pelangganTerpilih);
        else if (pilihan == 4) layananCekRutin(pelangganTerpilih);
        else if (pilihan == 5) tambahSaldoPelanggan();
    }
}


// menu pelanggan utama (WASD)
void menuPelanggan() {
    const char *opsiMenu[] = {
        "Tambah pelanggan",
        "Daftar pelanggan",
        "Pilih pelanggan untuk layanan",
        "Kembali"
    };
    int jumlahOpsi = (int)(sizeof(opsiMenu) / sizeof(opsiMenu[0]));

    while (1) {
        int pilihan = pilihDenganWASD("Manajemen Pelanggan", opsiMenu, jumlahOpsi);
        if (pilihan < 0 || pilihan == 3) break;
        if (pilihan == 0) tambahPelanggan();
        else if (pilihan == 1) tampilPelangganRingkas();
        else if (pilihan == 2) {
            bersihkanLayar();
            tampilJudulKotak("Pilih Pelanggan");
            char nomorTelepon[panjangString];
            printf("Masukkan nomor telepon: ");
            fgets(nomorTelepon, panjangString, stdin);
            nomorTelepon[strcspn(nomorTelepon, "\n")] = 0;
            int posisiPelanggan = cariPelangganTelepon(nomorTelepon);
            if (posisiPelanggan < 0) {
                printf("Pelanggan tidak ditemukan.\n");
                tekanEnter();
            } else {
                menuLayananPelanggan(posisiPelanggan);
            }
        }
    }
}


// menu booking (WASD)
void menuBooking() {
    const char *opsiMenu[] = {
        "Lihat slot per hari",
        "Buat slot mingguan",
        "Kembali"
    };
    int jumlahOpsi = (int)(sizeof(opsiMenu) / sizeof(opsiMenu[0]));

    while (1) {
        int pilihan = pilihDenganWASD("Manajemen Booking", opsiMenu, jumlahOpsi);
        if (pilihan < 0 || pilihan == 2) break;
        if (pilihan == 0) {
            const char *opsiHari[] = { "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
            int pilihanHari = pilihDenganWASD("Pilih Hari", opsiHari, 6);
            if (pilihanHari < 0) continue;
            bersihkanLayar();
            tampilSlotHari(opsiHari[pilihanHari]);
            garisPemisah();
            tekanEnter();
        } else if (pilihan == 1) {
            buatSlotMinggu();
            printf("Slot mingguan dibuat. Beberapa slot padat otomatis.\n");
            tekanEnter();
        }
    }
}


// menu inventaris (WASD)
void tampilInventarisSemua() {
    bersihkanLayar();
    tampilJudulKotak("Daftar Inventaris");
    for (int i = 0; i < jumlahInventaris; i++) {
        char hargaRupiah[panjangRupiah];
        formatRupiah(dataInventaris[i].harga, hargaRupiah, sizeof(hargaRupiah));
        printf("%d. %s | %s | %s | Stok: %d\n",
            i + 1,
            dataInventaris[i].namaItem,
            dataInventaris[i].merekPlesetan,
            hargaRupiah,
            dataInventaris[i].stok
        );
    }
    garisPemisah();
    tekanEnter();
}

void cariInventarisNama() {
    bersihkanLayar();
    tampilJudulKotak("Cari Inventaris Nama");
    char kataKunci[panjangString];
    printf("Masukkan kata kunci: ");
    fgets(kataKunci, panjangString, stdin);
    kataKunci[strcspn(kataKunci, "\n")] = 0;

    int jumlahHasil = 0;
    for (int i = 0; i < jumlahInventaris; i++) {
        if (cocokSubstring(dataInventaris[i].namaItem, kataKunci)) {
            char hargaRupiah[panjangRupiah];
            formatRupiah(dataInventaris[i].harga, hargaRupiah, sizeof(hargaRupiah));
            printf("%d. %s | %s | %s | Stok: %d\n",
                i + 1,
                dataInventaris[i].namaItem,
                dataInventaris[i].merekPlesetan,
                hargaRupiah,
                dataInventaris[i].stok
            );
            jumlahHasil++;
        }
    }
    if (!jumlahHasil) printf("Tidak ada hasil.\n");
    garisPemisah();
    tekanEnter();
}

void menuInventaris() {
    const char *opsiMenu[] = {
        "Tampilkan semua inventaris",
        "Urutkan harga naik",
        "Urutkan harga turun",
        "Cari inventaris nama",
        "Kembali"
    };
    int jumlahOpsi = (int)(sizeof(opsiMenu) / sizeof(opsiMenu[0]));

    while (1) {
        int pilihan = pilihDenganWASD("Inventaris dan Spare Part", opsiMenu, jumlahOpsi);
        if (pilihan < 0 || pilihan == 4) break;
        if (pilihan == 0) tampilInventarisSemua();
        else if (pilihan == 1) { urutInventarisNaik(); simpanInventaris(); }
        else if (pilihan == 2) { urutInventarisTurun(); simpanInventaris(); }
        else if (pilihan == 3) cariInventarisNama();
    }
}


// homepage & riwayat
void tampilHomepage() {
    bersihkanLayar();
    tampilJudulKotak("Bengkel Motor YONKJAWA - Homepage");
    printf("Deskripsi:\n");
    printf("YONKJAWA adalah bengkel motor profesional dengan budaya anak motor yang kuat.\n");
    printf("Kami spesialis motor di bawah 500cc dengan layanan lengkap yang realistik.\n\n");
    garisPemisah();
    printf("Alamat:\n");
    printf("Jalan Kuda Besi No. 99, Andir, Bandung, Jawa Barat\n\n");
    garisPemisah();
    printf("Sertifikat Bengkel (diakui):\n");
    printf("- Sertifikat Mekanik Profesional YONKJAWA\n");
    printf("- Sertifikat Sistem Injeksi dan Karburator\n");
    printf("- Sertifikat Suspensi dan Pengereman\n\n");
    garisPemisah();
    printf("Nomor Telepon Booking:\n");
    printf("022-1234567 (Senin - Sabtu 08:00 - 18:00, Minggu libur)\n");
    garisPemisah();
    tekanEnter();
}

void tampilRiwayat() {
    bersihkanLayar();
    tampilJudulKotak("Riwayat Order");
    for (int i = 0; i < jumlahOrder; i++) {
        char biayaRupiah[panjangRupiah];
        formatRupiah(dataOrder[i].totalBiaya, biayaRupiah, sizeof(biayaRupiah));
        printf("%d. %s (%s) | %s %02d:00 | %s | Biaya: %s\n",
            i + 1,
            dataOrder[i].namaPelanggan,
            dataOrder[i].nomorTelepon,
            dataOrder[i].hariBooking,
            dataOrder[i].jamBooking,
            dataOrder[i].deskripsiOrder,
            biayaRupiah
        );
    }
    garisPemisah();
    tekanEnter();
}


// menu utama (WASD)
void menuUtama() {
    const char *opsiMenuUtama[] = {
        "Homepage",
        "Manajemen pelanggan",
        "Booking jadwal",
        "Inventaris",
        "Riwayat order",
        "Keluar"
    };
    int jumlahOpsi = (int)(sizeof(opsiMenuUtama) / sizeof(opsiMenuUtama[0]));

    while (1) {
        int pilihan = pilihDenganWASD("Bengkel Motor YONKJAWA - Menu Utama", opsiMenuUtama, jumlahOpsi);
        if (pilihan < 0 || pilihan == 5) break;
        if (pilihan == 0) tampilHomepage();
        else if (pilihan == 1) menuPelanggan();
        else if (pilihan == 2) menuBooking();
        else if (pilihan == 3) menuInventaris();
        else if (pilihan == 4) tampilRiwayat();
    }
}


// muat semua data
void muatSemuaData() {
    muatPelanggan();
    muatOrder();
    muatBooking();
    muatInventaris();
    if (jumlahInventaris == 0) {
        tambahInventaris();
        simpanInventaris();
    }
    if (jumlahBooking == 0) {
        buatSlotMinggu();
    }
}

int main() {
    muatSemuaData();
    menuUtama();
    bersihkanLayar();
    printf("Terima kasih menggunakan Bengkel Motor YONKJAWA.\n");
    return 0;
}