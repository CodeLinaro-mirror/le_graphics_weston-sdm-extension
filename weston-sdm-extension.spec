Name: weston-sdm-extension
Version: 1.0
Release: r0
Summary: Header files for weston-sdm-extension
License: BSD-3-Clause & BSD-3-Clause-Clear
URL: http://support.cdmatech.com
Source0: %{name}-%{version}.tar.gz

%global apiver 10

BuildRequires: meson cairo pixman
BuildRequires: libxkbcommon libgbm libgbm-dev libinput
BuildRequires: display-hal-dev display-hal display-noship display-ship-dev
BuildRequires: libwayland-egl libwayland-server libwayland-client wayland-devel wayland-protocols-devel
BuildRequires: weston weston-libs weston-devel
BuildRequires: bootkpi-logging

%description
Provides QTI specific header files

%global debug_package %{nil}

%prep
%autosetup -n %{name}


%build
export LDFLAGS="%{?build_ldflags} -Wl,-z,undefs -Wl,-rpath,%{?_libdir}/libweston-%{?apiver}"
export CPPFLAGS="-I/usr/include -I/usr/include/core -I/usr/include/drm -I/usr/include/gbm -fno-operator-names"
export CFLAGS="-I/usr/include/gbm"
%meson
%meson_build

%install
%meson_install


%files
%dir %{_libdir}/libweston-%{apiver}
%{_libdir}/libweston-%{apiver}/drm-backend.so
%{_libdir}/libweston-%{apiver}/sdm-service.so
%{_libdir}/libweston-%{apiver}/gbm-buffer-backend.so
%{_libdir}/libweston-%{apiver}/screen-capture.so

