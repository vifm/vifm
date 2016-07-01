# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

EGIT_REPO_URI="git://github.com/vifm/vifm.git"
if [[ ${PV} == "9999" ]]; then
	KEYWORDS=""
	GIT_ECLASS="git-2"
else
	KEYWORDS="~x86 ~amd64 ~x86-fbsd ~x86-linux ~amd64-linux ~arm ~ppc ~s390"
	GIT_ECLASS=""
	SRC_URI="mirror://sourceforge/vifm/${P}.tar.bz2"
fi

inherit 'base' 'vim-doc' ${GIT_ECLASS}


DESCRIPTION="Console file manager with vi(m)-like keybindings"
HOMEPAGE="https://vifm.info/"

LICENSE="GPL-2"
SLOT="0"

IUSE="X +extended-keys gtk +magic vim vim-syntax"

DEPEND="
	>=sys-libs/ncurses-5.7-r7
	magic? ( sys-apps/file )
	gtk? ( x11-libs/gtk+:2 )
	X? ( x11-libs/libX11 )
"
RDEPEND="
	${DEPEND}
	vim? ( || ( app-editors/vim app-editors/gvim ) )
"

DOCS=( AUTHORS TODO README )


src_configure() {
	econf \
		$(use_enable extended-keys) \
		$(use_with magic libmagic) \
		$(use_with gtk) \
		$(use_with X X11)
}

src_install() {
	base_src_install

	if use vim; then
		local t
		for t in doc plugin; do
			insinto /usr/share/vim/vimfiles/"${t}"
			doins "${S}"/data/vim/"${t}"/"${PN}".*
		done
	fi

	if use vim-syntax; then
		local t
		for t in ftdetect ftplugin syntax; do
			insinto /usr/share/vim/vimfiles/"${t}"
			doins "${S}"/data/vim/"${t}"/"${PN}".vim
		done
	fi
}

pkg_postinst() {
	if use vim; then
		update_vim_helptags

		if [[ -n ${REPLACING_VERSIONS} ]]; then
			elog
			elog "You don't need to copy or link any files for"
			elog "  the vim plugin and documentation to work anymore."
			elog "If you copied any vifm files to ~/.vim/ manually"
			elog "  in earlier vifm versions, please delete them."
		fi
		elog
		elog "To use vim in vifm to view the documentation"
		elog "  edit ~/.vifm/vifmrc and set vimhelp instead of novimhelp"
		elog
	fi
}

pkg_postrm() {
	use vim && update_vim_helptags
}
