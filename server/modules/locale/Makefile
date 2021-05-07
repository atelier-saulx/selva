SUPPORTED_LOCALES := \
    af_ZA \
    am_ET \
    be_BY \
    bg_BG \
    ca_ES \
    cs_CZ \
    da_DK \
    de_DE \
    el_GR \
    en_GB \
    es_ES \
    et_EE \
    eu_ES \
    fi_FI \
    fr_FR \
    gsw_CH \
    he_IL \
    hr_HR \
    hu_HU \
    hy_AM \
    is_IS \
    it_IT \
    ja_JP \
    kk_KZ \
    ko_KR \
    lt_LT \
	nn_NO \
    nb_NO \
    nl_NL \
    pl_PL \
    pt_PT \
    ro_RO \
    ru_RU \
    sk_SK \
    sl_SI \
    sr_ME \
    sr_RS \
    sv_SE \
    tr_TR \
    uk_UA \
    zh_CN

LOCPATH ?= ../binaries/linux_x64/locale
LOCALES := $(patsubst %,%.UTF-8,$(SUPPORTED_LOCALES))
LOCALE_RES := $(addprefix $(LOCPATH)/,$(LOCALES))

export I18NPATH = ./localedata

all: $(LOCALE_RES)

$(LOCALE_RES):
	@mkdir -p $(LOCPATH)
	localedef -c --no-archive --no-hard-links -f UTF-8 -i $(subst .UTF-8,,$(notdir $@)) $@

clean:
	$(RM) -r cache
