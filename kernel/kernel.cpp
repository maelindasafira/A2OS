using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;

static volatile u16* const VGA = reinterpret_cast<volatile u16*>(0xB8000);
static u8 COLOR = 0x0F;
static int cursor_x = 0;
static int cursor_y = 0;

// Status keyboard dibuat global agar tetap konsisten di shell dan menu diagnosis.
static bool shift_down = false;
static bool caps_lock = false;

static inline u8 inb(u16 port) {
    u8 value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(u16 port, u8 value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outl(u16 port, u32 value) {
    asm volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

// Perbarui posisi hardware cursor VGA (mode teks 80x25).
static void update_cursor() {
    if (cursor_x < 0) cursor_x = 0;
    if (cursor_x > 79) cursor_x = 79;
    if (cursor_y < 0) cursor_y = 0;
    if (cursor_y > 24) cursor_y = 24;

    u16 position = static_cast<u16>(cursor_y * 80 + cursor_x);

    outb(0x3D4, 0x0F);
    outb(0x3D5, static_cast<u8>(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, static_cast<u8>((position >> 8) & 0xFF));
}

// Aktifkan cursor dan atur bentuknya agar terlihat.
static void enable_cursor() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x00);
    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0F);
    update_cursor();
}

static void scroll_screen() {
    for (int i = 0; i < 24 * 80; ++i)
        VGA[i] = VGA[i + 80];

    for (int i = 24 * 80; i < 25 * 80; ++i)
        VGA[i] = (static_cast<u16>(COLOR) << 8) | ' ';

    cursor_y = 24;
}

static void clear_screen() {
    for (int i = 0; i < 80 * 25; ++i)
        VGA[i] = (static_cast<u16>(COLOR) << 8) | ' ';

    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

static void put_char(char c) {
    if (c == '\n') {
        cursor_x = 0;
        ++cursor_y;
    } else if (c == '\r') {
        cursor_x = 0;
    } else {
        if (cursor_x >= 80) {
            cursor_x = 0;
            ++cursor_y;
        }

        if (cursor_y >= 25)
            scroll_screen();

        VGA[cursor_y * 80 + cursor_x] =
            (static_cast<u16>(COLOR) << 8) | static_cast<u8>(c);
        ++cursor_x;
    }

    if (cursor_y >= 25)
        scroll_screen();

    update_cursor();
}

static void print(const char* s) {
    while (*s)
        put_char(*s++);
}

static bool equals(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static void print_logo() {
    u8 old = COLOR;
    COLOR = 0x0B;
    print("      _    ____   ___  ____  \n");
    print("     / \\  |___ \\ / _ \\/ ___| \n");
    print("    / _ \\   __) | | | \\___ \\ \n");
    print("   / ___ \\ / __/| |_| |___) |\n");
    print("  /_/   \\_\\_____|\\___/|____/ \n\n");
    COLOR = old;
}

// Peta scancode keyboard PS/2 dengan layout US.
static char scancode_to_ascii(u8 code, bool shift, bool caps) {
    static const char normal[128] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
        'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
        'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
    };

    static const char shifted[128] = {
        0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
        '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
        'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
        'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
    };

    if (code >= 128)
        return 0;

    char c = normal[code];

    if (c >= 'a' && c <= 'z') {
        if (shift != caps)
            c = shifted[code];
    } else if (shift) {
        c = shifted[code];
    }

    return c;
}

// Membaca satu baris dari keyboard. Mendukung Enter dan Backspace.
static void read_line(char* buffer, int max_length) {
    int length = 0;
    buffer[0] = '\0';

    while (true) {
        if ((inb(0x64) & 1) == 0)
            continue;

        u8 scan = inb(0x60);

        if (scan == 0x2A || scan == 0x36) {
            shift_down = true;
            continue;
        }

        if (scan == 0xAA || scan == 0xB6) {
            shift_down = false;
            continue;
        }

        if (scan == 0xE0)
            continue;

        if (scan == 0x3A) {
            caps_lock = !caps_lock;
            continue;
        }

        if (scan & 0x80)
            continue;

        if (scan == 0x1C) {
            put_char('\n');
            buffer[length] = '\0';
            return;
        }

        char c = scancode_to_ascii(scan, shift_down, caps_lock);

        if (c == '\b') {
            if (length > 0) {
                --length;

                if (cursor_x > 0) {
                    --cursor_x;
                } else if (cursor_y > 0) {
                    --cursor_y;
                    cursor_x = 79;
                }

                VGA[cursor_y * 80 + cursor_x] =
                    (static_cast<u16>(COLOR) << 8) | ' ';
                update_cursor();
            }
        } else if (c >= 32 && c <= 126 && length < max_length - 1) {
            buffer[length++] = c;
            put_char(c);
        }
    }
}

static int read_number(const char* prompt) {
    char input[16];

    print(prompt);
    read_line(input, 16);

    if (input[0] >= '0' && input[0] <= '9')
        return input[0] - '0';

    return -1;
}

static bool ask_yes_no(const char* question) {
    char answer[8];

    print(question);
    print(" (y/n): ");
    read_line(answer, 8);

    return answer[0] == 'y' || answer[0] == 'Y';
}

static void show_help() {
    print("Perintah yang tersedia:\n");
    print("  help      - menampilkan daftar perintah\n");
    print("  about     - informasi tentang A2OS\n");
    print("  diagnosis - membuka asisten diagnosis\n");
    print("  clear     - membersihkan layar\n");
    print("  echo      - menampilkan teks\n");
    print("  shutdown  - mematikan A2OS (hanya di QEMU)\n");
    print("  exit      - keluar dari A2OS dan QEMU\n\n");
}

// Hanya bekerja jika QEMU dijalankan dengan perangkat isa-debug-exit.
static void shutdown_qemu() {
    print("A2OS sedang dimatikan...\n");
    print("Terima kasih telah menggunakan A2OS!\n");

    outl(0xF4, 0x10);

    for (;;)
        asm volatile ("cli; hlt");
}

// 0 = Ringan, 1 = Sedang, 2 = Berat
static int get_severity(int yes_count, bool answer_three) {
    if (answer_three || yes_count >= 3)
        return 2;
    if (yes_count == 2)
        return 1;
    return 0;
}

static void print_severity(int yes_count, bool answer_three) {
    int level = get_severity(yes_count, answer_three);
    u8 old = COLOR;

    static const char* labels[] = { "RINGAN", "SEDANG", "BERAT" };
    static const u8 colors[]    = { 0x0A,     0x0E,     0x0C   };   // hijau, kuning, merah

    print("Tingkat   : ");
    COLOR = colors[level];
    print(labels[level]);

    print("  [");
    for (int i = 0; i < 3; ++i) {
        if (i <= level)
            put_char(static_cast<char>(0xDB));   // blok terisi
        else
            put_char('.');
    }
    print("]\n");
    COLOR = old;

    if (level == 2)
        print("Prioritas : Segera periksa ke teknisi.\n");
    else if (level == 1)
        print("Prioritas : Jadwalkan pemeriksaan.\n");
    else
        print("Prioritas : Pantau dulu, belum mendesak.\n");
}

static void show_diagnosis_result(int device, int category, int yes_count, bool answer_three) {
    print("\n========== HASIL ANALISIS AWAL ==========\n");

    if (device == 1)
        print("Perangkat : Handphone\n");
    else
        print("Perangkat : Laptop\n");

    if (category == 1) {
        print("Kategori  : Baterai / daya\n");
        if (answer_three) {
            print("Temuan    : Ada indikasi kondisi baterai tidak normal.\n");
            print("Saran     : Hentikan penggunaan jika baterai tampak membengkak atau terlalu panas.\n");
            print("            Minta teknisi memeriksa baterai dan rangkaian daya.\n");
        } else if (yes_count >= 2) {
            print("Kemungkinan: Kapasitas baterai menurun atau konsumsi daya berlebih.\n");
            print("Saran      : Periksa kesehatan baterai dan aplikasi yang banyak memakai daya.\n");
        } else {
            print("Kemungkinan: Gangguan daya belum dapat dipastikan dari gejala ini.\n");
            print("Saran      : Periksa adaptor, konektor, dan kondisi baterai lebih lanjut.\n");
        }
    } else if (category == 2) {
        print("Kategori  : Pengisian daya\n");
        if (yes_count >= 2) {
            print("Kemungkinan: Kabel/adaptor, konektor, atau jalur pengisian bermasalah.\n");
            print("Saran      : Uji dengan aksesori pengisi daya yang sesuai dan teruji.\n");
            print("            Jika tetap gagal, periksa konektor dan rangkaian charging.\n");
        } else {
            print("Kemungkinan: Gangguan pengisian belum terlihat jelas.\n");
            print("Saran      : Amati apakah pengisian terputus-putus dan periksa aksesori.\n");
        }
    } else if (category == 3) {
        print("Kategori  : Layar / tampilan\n");
        if (yes_count >= 2) {
            print("Kemungkinan: Panel layar, kabel fleksibel, atau pengaturan tampilan bermasalah.\n");
            print("Saran      : Periksa kondisi fisik layar dan lakukan pemeriksaan komponen.\n");
        } else {
            print("Kemungkinan: Gangguan tampilan mungkin terkait perangkat lunak atau koneksi layar.\n");
            print("Saran      : Coba mulai ulang perangkat dan amati apakah gejala berulang.\n");
        }
    } else if (category == 4) {
        print("Kategori  : Performa lambat\n");
        if (yes_count >= 2) {
            print("Kemungkinan: Beban aplikasi tinggi, penyimpanan hampir penuh, atau sistem bermasalah.\n");
            print("Saran      : Periksa ruang penyimpanan, aplikasi berjalan, dan pembaruan sistem.\n");
        } else {
            print("Kemungkinan: Perlambatan belum menunjukkan satu penyebab yang jelas.\n");
            print("Saran      : Pantau aplikasi yang berjalan dan mulai ulang perangkat.\n");
        }
    } else if (category == 5) {
        if (device == 1)
            print("Kategori  : Layar sentuh\n");
        else
            print("Kategori  : Keyboard / touchpad\n");

        if (yes_count >= 2) {
            print("Kemungkinan: Komponen input atau driver/perangkat lunak mengalami gangguan.\n");
            print("Saran      : Periksa pengaturan/driver dan lakukan pengujian komponen.\n");
        } else {
            print("Kemungkinan: Gangguan input belum dapat dipastikan.\n");
            print("Saran      : Mulai ulang perangkat dan uji kembali fungsi input.\n");
        }
    } else {
        print("Kategori  : Sistem operasi / booting\n");
        if (yes_count >= 2) {
            print("Kemungkinan: Sistem operasi, file sistem, atau proses boot mengalami gangguan.\n");
            print("Saran      : Catat pesan error dan lakukan pemeriksaan sistem terlebih dahulu.\n");
        } else {
            print("Kemungkinan: Gangguan sistem belum menunjukkan penyebab yang pasti.\n");
            print("Saran      : Periksa pesan error dan perubahan terakhir pada perangkat.\n");
        }
    }

    print("-----------------------------------------\n");
    print_severity(yes_count, answer_three);
    print("-----------------------------------------\n");
    print("Analisis awal berdasarkan jawaban pelanggan.\n\n");
}

static void run_diagnosis() {
    int device = -1;
    int category = -1;

    clear_screen();
    print_logo();
    print("        A2OS - ASISTEN DIAGNOSIS TEKNISI\n\n");

    print("Pilih perangkat:\n");
    print("  1. Handphone\n");
    print("  2. Laptop\n");
    print("  0. Kembali\n\n");

    while (device < 0 || device > 2) {
        device = read_number("Pilihan: ");
        if (device < 0 || device > 2)
            print("Pilihan tidak valid. Masukkan 0, 1, atau 2.\n");
    }

    if (device == 0)
        return;

    clear_screen();
    print("PILIH KATEGORI MASALAH\n\n");
    print("  1. Baterai / perangkat cepat mati\n");
    print("  2. Pengisian daya / tidak bisa charging\n");
    print("  3. Layar / tampilan\n");
    print("  4. Performa lambat\n");
    if (device == 1)
        print("  5. Layar sentuh tidak responsif\n");
    else
        print("  5. Keyboard / touchpad bermasalah\n");
    print("  6. Sistem operasi / gagal booting\n");
    print("  0. Kembali\n\n");

    while (category < 0 || category > 6) {
        category = read_number("Kategori: ");
        if (category < 0 || category > 6)
            print("Pilihan tidak valid. Masukkan angka 0 sampai 6.\n");
    }

    if (category == 0)
        return;

    clear_screen();
    print("PERTANYAAN DIAGNOSIS\n");
    print("Jawab berdasarkan keterangan pelanggan.\n\n");

    int yes_count = 0;
    bool answer_three = false;

    if (category == 1) {
        if (ask_yes_no("1. Apakah baterai cepat habis atau perangkat sering mati?"))
            ++yes_count;
        if (ask_yes_no("2. Apakah persentase baterai berubah tidak wajar?"))
            ++yes_count;
        answer_three = ask_yes_no("3. Apakah baterai terlihat membengkak atau terasa sangat panas?");
        if (answer_three) ++yes_count;
    } else if (category == 2) {
        if (ask_yes_no("1. Apakah perangkat tidak mengisi daya dengan aksesori yang biasa dipakai?"))
            ++yes_count;
        if (ask_yes_no("2. Apakah pengisian sering tersambung dan terputus?"))
            ++yes_count;
        if (ask_yes_no("3. Apakah perangkat tetap tidak mengisi dengan kabel/adaptor lain yang sesuai?"))
            ++yes_count;
    } else if (category == 3) {
        if (ask_yes_no("1. Apakah layar gelap atau tidak menampilkan gambar dengan normal?"))
            ++yes_count;
        if (ask_yes_no("2. Apakah muncul garis, kedipan, atau perubahan warna?"))
            ++yes_count;
        if (ask_yes_no("3. Apakah masalah tetap muncul setelah perangkat dimulai ulang?"))
            ++yes_count;
    } else if (category == 4) {
        if (ask_yes_no("1. Apakah perangkat terasa lambat saat membuka aplikasi?"))
            ++yes_count;
        if (ask_yes_no("2. Apakah ruang penyimpanan hampir penuh?"))
            ++yes_count;
        if (ask_yes_no("3. Apakah perangkat tetap lambat setelah dimulai ulang?"))
            ++yes_count;
    } else if (category == 5) {
        if (device == 1) {
            if (ask_yes_no("1. Apakah sebagian area layar sentuh tidak merespons?"))
                ++yes_count;
            if (ask_yes_no("2. Apakah layar sentuh bergerak atau memilih menu sendiri?"))
                ++yes_count;
            if (ask_yes_no("3. Apakah masalah tetap terjadi setelah layar dibersihkan dan perangkat dimulai ulang?"))
                ++yes_count;
        } else {
            if (ask_yes_no("1. Apakah beberapa tombol keyboard atau touchpad tidak berfungsi?"))
                ++yes_count;
            if (ask_yes_no("2. Apakah masalah terjadi juga pada perangkat input lain?"))
                ++yes_count;
            if (ask_yes_no("3. Apakah masalah tetap terjadi setelah laptop dimulai ulang?"))
                ++yes_count;
        }
    } else {
        if (ask_yes_no("1. Apakah perangkat gagal masuk ke sistem operasi?"))
            ++yes_count;
        if (ask_yes_no("2. Apakah muncul pesan error atau proses boot berhenti?"))
            ++yes_count;
        if (ask_yes_no("3. Apakah masalah mulai muncul setelah update atau perubahan sistem?"))
            ++yes_count;
    }

    show_diagnosis_result(device, category, yes_count, answer_three);
    print("Tekan Enter untuk kembali ke menu utama...");
    char wait[4];
    read_line(wait, 4);
}

static void show_about() {
    print("A2OS - Mini OS sederhana berbasis kernel C++.\n");
    print("A2OS dikembangkan sebagai asisten diagnosis awal\n");
    print("untuk membantu teknisi mengelompokkan keluhan HP dan laptop.\n");
}

static void shell() {
    char input[128];

    print("Welcome to A2OS!\n");
    print("Ketik 'help' untuk melihat daftar perintah.\n\n");

    while (true) {
        print("A2OS> ");
        read_line(input, 128);

        if (equals(input, "help")) {
            show_help();
        } else if (equals(input, "about")) {
            show_about();
        } else if (equals(input, "diagnosis")) {
            run_diagnosis();
        } else if (equals(input, "clear")) {
            clear_screen();
            print_logo();
            print("Ketik 'help' untuk melihat perintah.\n\n");
        } else if (input[0] == 'e' && input[1] == 'c' &&
                input[2] == 'h' && input[3] == 'o' &&
                input[4] == ' ') {
            print(input + 5);
            print("\n\n");
        } else if (equals(input, "shutdown") || equals(input, "exit")) {
            shutdown_qemu();
        } else if (input[0] == '\0') {
            // Abaikan baris kosong.
        } else {
            print("Perintah tidak dikenal. Ketik 'help'.\n\n");
        }
    }
}

static void main_menu() {
    while (true) {
        clear_screen();
        print_logo();
        print("        ASISTEN DIAGNOSIS TEKNISI\n\n");
        print("  1. Mulai diagnosis perangkat\n");
        print("  2. Terminal A2OS\n");
        print("  3. Tentang A2OS\n");
        print("  4. Matikan A2OS\n\n");

        int choice = read_number("Pilih menu: ");

        if (choice == 1) {
            run_diagnosis();
        } else if (choice == 2) {
            clear_screen();
            print_logo();
            shell();
        } else if (choice == 3) {
            clear_screen();
            print_logo();
            show_about();
            print("Tekan Enter untuk kembali...");
            char wait[4];
            read_line(wait, 4);
        } else if (choice == 4) {
            shutdown_qemu();
        } else {
            print("Pilihan tidak valid. Tekan Enter untuk mencoba lagi...");
            char wait[4];
            read_line(wait, 4);
        }
    }
}

extern "C" void kernel_main() {
    clear_screen();
    enable_cursor();

    print_logo();
    print("[ OK ] Kernel A2OS berhasil dimuat.\n");
    print("[ OK ] Shell internal siap digunakan.\n");
    print("[ OK ] Asisten diagnosis siap digunakan.\n\n");

    main_menu();

    for (;;)
        asm volatile ("cli; hlt");
}
