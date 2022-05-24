" For ALE users
let g:ale_c_cc_options .= " -iquote .. " . trim(system('pkg-config --cflags gtk+-3.0 gio-unix-2.0 gudev-1.0 xext libpulse libpulse-mainloop-glib'))
let g:ale_cpp_cc_options .= " -iquote .. " . trim(system('pkg-config --cflags gtk+-3.0 gio-unix-2.0 gudev-1.0 xext libpulse libpulse-mainloop-glib'))
