# A2OS — tahap pertama

Prototype kernel 32-bit x86 dengan C++ dan NASM. Boot melalui GRUB Multiboot,
menampilkan logo ASCII A2OS, lalu menyediakan shell internal dengan perintah
`help`, `about`, `clear`, dan `echo`.

## 1. Install dependency di Ubuntu WSL2

```bash
sudo apt update
sudo apt install build-essential g++-multilib nasm grub-pc-bin grub-common xorriso qemu-system-x86 -y
```

## 2. Build

Jalankan dari folder proyek:

```bash
make
```

Periksa header Multiboot:

```bash
make check
```

## 3. Jalankan di QEMU

```bash
make run
```

Shell menerima input dari keyboard QEMU. Klik jendela QEMU jika keyboard belum
terfokus. Keluar dari QEMU dengan `Ctrl+Alt+G` atau tutup jendelanya.

## Catatan

- Ini adalah prototype awal untuk memenuhi demonstrasi boot, logo ASCII, dan
  shell internal; belum mengimplementasikan scheduler, mutex, allocator,
  filesystem, atau proteksi akses.
- Kernel berjalan dalam mode x86 32-bit dan memakai VGA text mode serta polling
  keyboard PS/2 untuk tahap awal.
- Jangan menganggap pesan boot sebagai bukti fitur yang belum diimplementasikan.
