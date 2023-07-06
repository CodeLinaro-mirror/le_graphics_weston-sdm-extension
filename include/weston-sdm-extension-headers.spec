Name: weston-sdm-extension-headers
Version: 1.0
Release: r0
Summary: Header files for weston-sdm-extension
License: BSD-3-Clause & BSD-3-Clause-Clear
URL: http://support.cdmatech.com
Source0: %{name}-%{version}.tar.gz

#BuildRequires: libxkbcommon libgbm-dev meson cairo display-hal-headers display-hal display-noship  display-ship libinput pixman adreno wayland-devel weston
BuildRequires: libgbm-dev 
%description
Provides QTI specific header files

%global debug_package %{nil}


%prep
%autosetup -n %{name}

%build
export LDFLAGS="%{?build_ldflags} -Wl,-z,undefs"

%install
mkdir -p %{buildroot}%{_includedir}
cp compositor-sdm-output.h %{buildroot}%{_includedir}
cp gbm-buffer-backend.h %{buildroot}%{_includedir}
cp screen-capture.h %{buildroot}%{_includedir}


%files
%{_includedir}/compositor-sdm-output.h
%{_includedir}/gbm-buffer-backend.h
%{_includedir}/screen-capture.h


