#ifdef FORCE_LIBINTL
#ifdef NLS_TRANSLATE_TR
#undef NLS_TRANSLATE_TR
#include "mxqt-settings.cc"
#define NLS_TRANSLATE_TR
#endif
#endif

#if !defined(NLS_TRANSLATE_TR) || !defined(FORCE_LIBINTL)
#include <iostream>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtCore/QCommandLineParser>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QLineEdit>
#include <QtGui/QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/QAction>
#else
#include <QtWidgets/QAction>
#endif
#include <unistd.h>
#include <assert.h>
#include "mxnls.h"
#include "country.c"
#include "qrc.cc"

#define EXIT_WINDOW_CLOSED (EXIT_FAILURE + EXIT_SUCCESS + 1)

static char const program_name[] = "mxqt-settings";
static char const program_version[] = "1.0";

#define _STRINGIFY1(x) #x
#define _STRINGIFY(x) _STRINGIFY1(x)

/*
 * C-data generated from preferences.xml
 */
#if defined(ENABLE_NLS) && !defined(FORCE_LIBINTL)
#include "preferences.xml.tr.c"
#else
#include "preferences.xml.c"
#endif

/*** ---------------------------------------------------------------------- ***/

/*
 * Utility functions
 */
static bool bool_from_string(const QString &str)
{
	if (str.isEmpty())
		return false;
	if (str.compare("YES", Qt::CaseInsensitive) == 0 ||
		str.compare("ON", Qt::CaseInsensitive) == 0 ||
		str.compare("TRUE", Qt::CaseInsensitive) == 0 ||
		str == "1")
		return true;
	return false;
}

/*** ---------------------------------------------------------------------- ***/

static const char *bool_to_string(bool b)
{
	return b ? "YES" : "NO";
}

/*** ---------------------------------------------------------------------- ***/

class Q_WIDGETS_EXPORT TranslatableCheckBox: public QCheckBox
{
public:
	TranslatableCheckBox(const char *label) : QCheckBox(_(label))
	{
		m_orig_text = U_(label);
	}
	void setToolTip(const char *tooltip)
	{
		m_orig_tooltip = U_(tooltip);
		QWidget::setToolTip(_(m_orig_tooltip));
	}
	void retranslate_ui(bool enableTooltips)
	{
		QCheckBox::setText(_(m_orig_text));
		if (enableTooltips)
			QWidget::setToolTip(_(m_orig_tooltip));
		else
			QWidget::setToolTip(QString());
	}
protected:
#ifdef FORCE_LIBINTL
	QByteArray m_orig_text;
	QByteArray m_orig_tooltip;
#else
	libnls_msgid_type m_orig_text;
	libnls_msgid_type m_orig_tooltip;
#endif
};

/*** ---------------------------------------------------------------------- ***/

class Q_WIDGETS_EXPORT PrefWidget
{
public:
	int type;
	bool gui_only;
	QByteArray m_section;
	QByteArray m_name;
	const char *element_name;

	PrefWidget(int type, const char *element_name, QString &section, const char *name, bool gui_only) :
		type(type),
		gui_only(gui_only),
		m_section(section.toUtf8().append('\0')),
		m_name(name),
		element_name(element_name)
	{
		if (name == NULL || *name == '\0')
		{
			// should not happen as it is already checked by xml-dump.pl
			qWarning("%s missing name", element_name);
		}
	}
	virtual const QString getPrefValue(void) = 0;
	virtual void setPrefValue(const QString &value) = 0;
	virtual void populatePreferences(void) = 0;
	virtual void updatePreferences(void) = 0;
	virtual void setOrigValue(void) = 0;
	virtual bool isChanged(void) = 0;
	const char *name(void) { return m_name.constData(); }
	const char *section(void) { return m_section.constData(); }
protected:
#ifdef FORCE_LIBINTL
	QByteArray m_orig_text;
	QByteArray m_orig_tooltip;
#else
	libnls_msgid_type m_orig_text;
	libnls_msgid_type m_orig_tooltip;
#endif
	void setToolTip(QWidget *parent, const char *tooltip)
	{
		m_orig_tooltip = U_(tooltip);
		parent->setToolTip(_(m_orig_tooltip));
	}
public:
	virtual void retranslate_ui(bool enableTooltips) = 0;
};

/*** ---------------------------------------------------------------------- ***/

class Q_WIDGETS_EXPORT PrefBool : public QCheckBox, public PrefWidget
{
public:
	PrefBool(QString &section, const char *name, const char *text, bool default_value, bool gui_only) :
		PrefWidget(TYPE_BOOL, "bool", section, name, gui_only),
		m_value(default_value),
		m_default_value(default_value),
		m_orig_value(default_value)
	{
		m_orig_text = U_(text);
		this->setObjectName(m_name);
		this->setText(_(m_orig_text));
	}
	virtual const QString getPrefValue(void) { return bool_to_string(m_value); }
	virtual void setPrefValue(const QString &v) { m_value = bool_from_string(v); }
	virtual void setOrigValue(void) { m_orig_value = m_value; }
	virtual bool isChanged(void) { return m_orig_value != m_value; }
	virtual void populatePreferences(void)
	{
		setChecked(m_value);
	}
	virtual void updatePreferences(void)
	{
		m_value = isChecked();
	}
private:
	bool m_value;
	bool m_default_value;
	bool m_orig_value;
public:
	void setToolTip(const char *tooltip) { PrefWidget::setToolTip(static_cast<QWidget *>(this), tooltip); }
public:
	virtual void retranslate_ui(bool enableTooltips)
	{
		this->setText(_(m_orig_text));
		if (enableTooltips)
			QWidget::setToolTip(_(m_orig_tooltip));
		else
			QWidget::setToolTip(QString());
	}
};

/*** ---------------------------------------------------------------------- ***/

/*
 * QSpinBox can only hold values of type int,
 * which is not good enoguh for a max memory_size of 0x80000000
 */
class Q_WIDGETS_EXPORT PrefInt : public QDoubleSpinBox, public PrefWidget
{
public:
	PrefInt(QString &section, const char *name, long default_value, bool gui_only) :
		PrefWidget(TYPE_INT, "int", section, name, gui_only),
		m_value(default_value),
		m_default_value(default_value),
		m_orig_value(default_value)
	{
		this->setObjectName(m_name);
		this->setDecimals(0);
	}
	virtual const QString getPrefValue(void)
	{
		QString s;
		if (type == TYPE_INT)
			s.setNum(m_value);
		else
			s.setNum((unsigned int)m_value);
		return s;
	}
	virtual void setPrefValue(const QString &v) { m_value = v.toLong(); }
	virtual void setOrigValue(void) { m_orig_value = m_value; }
	virtual bool isChanged(void) { return m_orig_value != m_value; }
	void setRange(long minimum, long maximum)
	{
		minval = minimum;
		maxval = maximum;
		QDoubleSpinBox::setRange(minimum, maximum);
	}
	virtual void populatePreferences(void)
	{
		setValue(m_value);
	}
	virtual void updatePreferences(void)
	{
		m_value = value();
	}
private:
	long m_value;
	long m_default_value;
	long m_orig_value;
	long minval;
	long maxval;
protected:
	QLabel *button_label;
public:
	void setToolTip(const char *tooltip) { PrefWidget::setToolTip(static_cast<QWidget *>(this), tooltip); }
	virtual void retranslate_ui(bool enableTooltips)
	{
		button_label->setText(_(m_orig_text));
		if (enableTooltips)
			QWidget::setToolTip(_(m_orig_tooltip));
		else
			QWidget::setToolTip(QString());
	}
public:
	void setButtonLabel(QLabel *label, const char *text)
	{
		m_orig_text = U_(text);
		button_label = label;
		label->setText(_(m_orig_text));
	}
};

/*** ---------------------------------------------------------------------- ***/

class Q_WIDGETS_EXPORT PrefString : public QLineEdit, public PrefWidget
{
public:
	PrefString(QString &section, const char *name, const QString &default_value, bool gui_only) :
		PrefWidget(TYPE_STRING, "string", section, name, gui_only),
		m_value(default_value),
		m_default_value(default_value),
		m_orig_value(default_value)
	{
		this->setObjectName(m_name);
	}
	virtual const QString getPrefValue(void) { return m_value; }
	virtual void setPrefValue(const QString &v) { m_value = v; }
	virtual void setOrigValue(void) { m_orig_value = m_value; }
	virtual bool isChanged(void) { return m_orig_value != m_value; }
	virtual void populatePreferences(void)
	{
		setText(getPrefValue());
	}
	virtual void updatePreferences(void)
	{
		setPrefValue(displayText());
	}
private:
	QString m_value;
	QString m_default_value;
	QString m_orig_value;
protected:
	QLabel *button_label;
public:
	void setToolTip(const char *tooltip) { PrefWidget::setToolTip(static_cast<QWidget *>(this), tooltip); }
	virtual void retranslate_ui(bool enableTooltips)
	{
		button_label->setText(_(m_orig_text));
		if (enableTooltips)
			QWidget::setToolTip(_(m_orig_tooltip));
		else
			QWidget::setToolTip(QString());
	}
public:
	void setButtonLabel(QLabel *label, const char *text)
	{
		m_orig_text = U_(text);
		button_label = label; 
		label->setText(_(m_orig_text));
	}
};

/*** ---------------------------------------------------------------------- ***/

class Q_WIDGETS_EXPORT PrefPath : public QPushButton, public PrefWidget
{
	Q_OBJECT

public:
	PrefPath(QString &section, const char *name, const QString &default_value, bool gui_only, bool isFolder = false) :
		PrefWidget(isFolder ? TYPE_FOLDER : TYPE_PATH, isFolder ? "folder" : "path", section, name, gui_only),
		info(default_value),
		m_default_value(default_value),
		m_orig_value(default_value),
		button_rdonly(nullptr),
		button_dosnames(nullptr),
		button_insensitive(nullptr),
		m_flags(NO_FLAGS)
	{
		this->setObjectName(m_name);
		connect(this, SIGNAL(clicked()), this, SLOT(select_path()));
	}
	virtual const QString getPrefValue(void) { return info.filePath(); }
	virtual void setPrefValue(const QString &v) { info.setFile(v); }
	virtual void setOrigValue(void) { m_orig_value = info.filePath(); m_orig_flags = m_flags; }
	virtual bool isChanged(void) { return m_orig_value != info.filePath() || m_orig_flags != m_flags; }
	const QString getFilename(void) { return info.fileName(); }
	void setFlags(int flags) { m_flags = flags; }
	int getFlags(void) { return m_flags; }
	bool hasFlags(void) { return m_flags != NO_FLAGS; }
	bool isReadonly(void) { return (m_flags & DRV_FLAG_RDONLY) != 0; }
	bool isDostrunc(void) { return (m_flags & DRV_FLAG_8p3) != 0; }
	bool isCaseInsensitive(void) { return (m_flags & DRV_FLAG_CASE_INSENS) != 0; }
private slots:
	void select_path(void);
private:
	QFileInfo info;
	QString m_default_value;
	QString m_orig_value;
public:
	TranslatableCheckBox *button_rdonly;
	TranslatableCheckBox *button_dosnames;
	TranslatableCheckBox *button_insensitive;
	int m_flags;
	int m_default_flags;
	int m_orig_flags;
	void setToolTip(const char *tooltip) { PrefWidget::setToolTip(static_cast<QWidget *>(this), tooltip); }
	virtual void populatePreferences(void)
	{
		setText(getFilename());
		if (hasFlags())
		{
			button_rdonly->setChecked(isReadonly());
			button_dosnames->setChecked(isDostrunc());
			button_insensitive->setChecked(isCaseInsensitive());
		}
	}
	virtual void updatePreferences(void)
	{
		/* pathname already done in select_path */
		if (hasFlags())
		{
			m_flags = 0;
			if (button_rdonly->isChecked())
				m_flags |= DRV_FLAG_RDONLY;

			if (button_dosnames->isChecked())
				m_flags |= DRV_FLAG_8p3;

			if (button_insensitive->isChecked())
				m_flags |= DRV_FLAG_CASE_INSENS;
		}
	}
protected:
	QLabel *button_label;
public:
	virtual void retranslate_ui(bool enableTooltips)
	{
		button_label->setText(_(m_orig_text));
		if (enableTooltips)
			QWidget::setToolTip(_(m_orig_tooltip));
		else
			QWidget::setToolTip(QString());
		if (hasFlags())
		{
			button_rdonly->retranslate_ui(enableTooltips);
			button_dosnames->retranslate_ui(enableTooltips);
			button_insensitive->retranslate_ui(enableTooltips);
		}
	}
public:
	void setButtonLabel(QLabel *label, const char *text)
	{
		m_orig_text = U_(text);
		button_label = label;
		label->setText(_(m_orig_text));
	}
};

/*** ---------------------------------------------------------------------- ***/

void PrefPath::select_path(void)
{
	QFileDialog dlg(this, this->type == TYPE_FOLDER ? _("Select a Folder") : _("Select a File"));
	if (this->type == TYPE_FOLDER)
	{
		dlg.setFileMode(QFileDialog::Directory);
		dlg.setOption(QFileDialog::ShowDirsOnly, true);
		dlg.setDirectory(info.filePath());
	} else
	{
		dlg.setFileMode(QFileDialog::AnyFile);
		dlg.selectFile(info.filePath());
	}
	if (dlg.exec())
	{
		QString filename = dlg.selectedFiles().first();
		if (!filename.isEmpty())
		{
			info.setFile(filename);
			setText(getFilename());
		}
	}
}

/*** ---------------------------------------------------------------------- ***/

class Q_WIDGETS_EXPORT PrefChoice : public QComboBox, public PrefWidget
{
	Q_OBJECT

public:
	PrefChoice(QString &section, const char *name, int default_value, bool gui_only) :
		PrefWidget(TYPE_CHOICE, "choice", section, name, gui_only),
		m_value(default_value),
		m_default_value(default_value),
		m_orig_value(default_value)
	{
		this->setObjectName(m_name);
	}
#ifdef FORCE_LIBINTL
	typedef struct { QByteArray orig_text; int value; } choiceItem;
#else
	typedef struct { libnls_msgid_type orig_text; int value; } choiceItem;
#endif
	QVector <choiceItem> items;
	virtual const QString getPrefValue(void) { return QString().setNum(items[m_value].value); }
	virtual void setPrefValue(const QString &v) { m_value = v.toInt(); }
	virtual void setOrigValue(void) { m_orig_value = m_value; }
	virtual bool isChanged(void) { return m_orig_value != items[m_value].value; }
public:
	int m_value;
	int m_default_value;
	int m_orig_value;
public:
	int minval;
	int maxval;
	void setToolTip(const char *tooltip) { PrefWidget::setToolTip(static_cast<QWidget *>(this), tooltip); }
	virtual void populatePreferences(void)
	{
		/*
		 * convert the value from preferences to the combobox index
		 */
		for (int i = 0; i < count(); i++)
		{
			int v = items[i].value;
			if (v == m_value)
			{
				m_value = i;
				break;
			}
		}
		setCurrentIndex(m_value);
	}
	virtual void updatePreferences(void)
	{
		m_value = currentIndex();
	}
	void addItem(const char *text, const QIcon &icon, int value)
	{
#ifdef FORCE_LIBINTL
		items.push_back({text, value});
		QComboBox::addItem(icon, _(text));
#else
		libnls_msgid_type msgid = U_(text);
		items.push_back({msgid, value});
		QComboBox::addItem(icon, _(msgid));
#endif
	}
protected:
	QLabel *button_label;
public:
	virtual void retranslate_ui(bool enableTooltips)
	{
		button_label->setText(_(m_orig_text));
		if (enableTooltips)
			QWidget::setToolTip(_(m_orig_tooltip));
		else
			QWidget::setToolTip(QString());
		for (int i = 0; i < items.length(); i++)
		{
			QComboBox::setItemText(i, _(items[i].orig_text));
		}
	}
public:
	void setButtonLabel(QLabel *label, const char *text)
	{
		m_orig_text = U_(text);
		button_label = label;
		label->setText(_(m_orig_text));
	}
};

/*** ---------------------------------------------------------------------- ***/

class Q_WIDGETS_EXPORT GuiWindow : public QMainWindow
{
	Q_OBJECT

public:
	GuiWindow(QWidget *parent);
	bool getPreferences(void);
	bool populatePreferences(void);
	bool writePreferences(void);
	bool updatePreferences(void);
	void parseXml(void);
	bool anyChanged(void);

	int exit_code;

	QTabWidget *notebook;
#ifdef FORCE_LIBINTL
	QStringList tabNames;
#else
	std::vector<libnls_msgid_type> tabNames;
#endif
	QStringList section_names;
	PrefChoice *language_choice;

	QString config_file;
	PrefBool *pref_show_tooltips;

	QString defaultConfig(void);

	void addSetting(PrefWidget *w) { widget_list.push_back(w); }

	void retranslate_ui(bool enableTooltips);

private slots:
	void ok_clicked(void);
	void cancel_clicked(void);
	void enableTooltips(void);
#ifdef ENABLE_NLS
	void language_changed(int index);
#endif

protected:
	QList<PrefWidget *> widget_list;
	bool evaluatePreferencesLine(const char *line, bool gui_only);
	bool eval_int(long &outval, long minval, long maxval, const char *&in);
	QString *eval_quotated_str(const char *&in);
	QString *eval_quotated_str_path(const char *&in);
	bool eval_bool(PrefWidget *w, const char *&line);

	virtual void closeEvent(QCloseEvent *ev) { exit_code = EXIT_WINDOW_CLOSED; ev->accept(); }
	
	void setWindowTitle(const char *title)
	{
		m_orig_title = U_(title);
		QMainWindow::setWindowTitle(_(m_orig_title));
	}
	void quitApp(void);
protected:
#ifdef FORCE_LIBINTL
	QByteArray m_orig_title;
#else
	libnls_msgid_type m_orig_title;
#endif
	QDialogButtonBox *button_box;
	QPushButton *ok_button;
	QPushButton *cancel_button;
};

/*** ---------------------------------------------------------------------- ***/

class Q_WIDGETS_EXPORT QGridWidget : public QWidget
{
	Q_OBJECT

public:
	QGridWidget(QWidget *parent = 0) : QWidget(parent)
	{
		setLayout(&layout); 
		layout.setSizeConstraint(QLayout::SetNoConstraint);
	}
	void addWidget(QWidget *widget, int row, int column, int rowSpan = 1, int columnSpan = 1, Qt::Alignment alignment = Qt::Alignment())
	{
		layout.addWidget(widget, row, column, rowSpan, columnSpan, alignment);
	}
private:
	QGridLayout layout;
};

/*** ---------------------------------------------------------------------- ***/

class Q_WIDGETS_EXPORT QBox : public QFrame
{
	Q_OBJECT

public:
	QBox(QBoxLayout::Direction direction = QBoxLayout::LeftToRight, QWidget *parent = 0) :
		QFrame(parent),
		boxlayout(direction)
	{
		setLayout(&boxlayout); 
	}
	void addWidget(QWidget *widget, int stretch = 0, Qt::Alignment alignment = Qt::Alignment())
	{
		boxlayout.addWidget(widget, stretch, alignment);
	}
	void addStretch()
	{
		boxlayout.addStretch(1);
	}
private:
	QBoxLayout boxlayout;
};

class Q_WIDGETS_EXPORT QHBox : public QBox
{
	Q_OBJECT

public:
	QHBox(QWidget *parent = 0) : QBox(QBoxLayout::LeftToRight, parent)
	{
	}
};

/*** ---------------------------------------------------------------------- ***/

class Q_WIDGETS_EXPORT QVBox : public QBox
{
	Q_OBJECT

public:
	QVBox(QWidget *parent = 0) : QBox(QBoxLayout::TopToBottom, parent)
	{
	}
};

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

/*
 * Utility functions
 */
static QString path_expand(const QString &value)
{
	if (value[0] == '~')
	{
		return QDir::homePath() + value.mid(1);
	} else
	{
		return value;
	}
}

/*** ---------------------------------------------------------------------- ***/

static QString path_shrink(const QString &value)
{
	QString home;
	
	if (value.isEmpty())
		return QString("");
	home = QDir::homePath();
	if (home.endsWith('/') || home.endsWith('\\'))
		home.chop(1); // removeLast
	if (!home.isEmpty() && value.startsWith(home))
		return QString("~") + value.mid(home.length());
	return QString(value);
}

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

/*
 * Read/write preferences
 */

#define ISSPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')

/** **********************************************************************************************
 *
 * @brief Write configuration file
 *
 * @param[in] cfgfile path
 *
 * @return true/false
 *
 ************************************************************************************************/
bool GuiWindow::writePreferences(void)
{
	QFile file(config_file);
	const char *last_section = nullptr;

	if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text))
	{
		QMessageBox dialog(QMessageBox::Critical,
			_(m_orig_title),
			QString::asprintf(_("can't create %s:\n%s"), config_file.toUtf8().constData(), file.errorString().toUtf8().constData()),
			QMessageBox::Cancel);
		dialog.exec();
		return false;
	}
	
	for (auto w: widget_list)
	{
		const char *section = w->section();
		const char *name = w->name();
		if (last_section == NULL || strcmp(section, last_section) != 0)
		{
			if (last_section != NULL)
				file.write("\n");
			file.write(QString::asprintf("[%s]\n", section).toUtf8().constData());
			last_section = section;
			/* Cosmetic: write back original comment from example configuration */
			if (strcmp(section, "ADDITIONAL ATARI DRIVES") == 0)
			{
				file.write("# atari_drv_<A..T,V..Z> = flags [1:read-only, 2:8+3, 4:case-insensitive] path or image\n");
			}
		}
		if (w->gui_only)
		{
			/*
			 * mark as comment, so emulator does not choke on it
			 */
			file.write("#.");
		}
		switch (w->type)
		{
		case TYPE_NONE:
			break;
		case TYPE_PATH:
		case TYPE_FOLDER:
			{
				PrefPath *p = dynamic_cast<PrefPath *>(w);
				QString str = path_shrink(w->getPrefValue());
				if (!str.isEmpty())
				{
					if (!p->hasFlags())
						file.write(QString("%1 = \"%2\"\n").arg(name).arg(str.toUtf8().constData()).toUtf8().constData());
					else
						file.write(QString("%1 = %2 \"%3\"\n").arg(name).arg(p->getFlags()).arg(str.toUtf8().constData()).toUtf8().constData());
				}
			}
			break;
		case TYPE_STRING:
			{
				QString str = w->getPrefValue();
				if (!str.isEmpty())
					file.write(QString("%1 = \"%2\"\n").arg(name).arg(str.toUtf8().constData()).toUtf8().constData());
			}
			break;
		case TYPE_INT:
			file.write(QString("%1 = %2\n").arg(name).arg(w->getPrefValue()).toUtf8().constData());
			break;
		case TYPE_UINT:
			file.write(QString("%1 = %2\n").arg(name).arg(w->getPrefValue()).toUtf8().constData());
			break;
		case TYPE_BOOL:
			file.write(QString("%1 = %2\n").arg(name).arg(w->getPrefValue()).toUtf8().constData());
			break;
		case TYPE_CHOICE:
			{
				QString value = w->getPrefValue();
				file.write(QString("%1 = %2\n").arg(name).arg(value).toUtf8().constData());
				/* Cosmetic: write back original comment from example configuration */
				if (strcmp(name, "atari_screen_colour_mode") == 0)
				{
					file.write("# 0:24b 1:16b 2:256 3:16 4:16ip 5:4ip 6:mono\n");
				}
			}
			break;
		default:
			assert(0);
		}
	}
	file.close();
	return true;
}

/** **********************************************************************************************
 *
 * @brief Read string, optionally enclosed in "" or ''
 *
 * @param[in_out] in input line pointer, will be advanced accordingly
 *
 * @return str or nullptr for error
 *
 ************************************************************************************************/

QString *GuiWindow::eval_quotated_str(const char *&in)
{
	QString *text;
	const char *in_start = in;
	char delimiter = *in;

	if (delimiter == '\"' || delimiter == '\'')
	{
		in_start++;
	} else
	{
		delimiter = 0;
	}
	text = new QString();
	while (*in_start != '\0' && *in_start != '\r' && *in_start != '\n' && *in_start != delimiter)
	{
		text->append(*in_start);
		in_start++;
	}

	if (delimiter != 0)
	{
		if (*in_start == delimiter)
		{
			in_start++;
		} else
		{
			delete text;
			text = nullptr;	/* missing delimiter */
		}
	}
	in = in_start;

	return text;
}

/** **********************************************************************************************
 *
 * @brief Read string, optionally enclosed in "" or '', and evaluates leading '~' for user home directory
 *
 * @param[out]    outbuf   output buffer, holds raw string
 * @param[in]     bufsiz   size of output buffer including end-of-string
 * @param[in_out] in       input line pointer, will be advanced accordingly
 *
 * @return str or nullptr for error
 *
 ************************************************************************************************/
QString *GuiWindow::eval_quotated_str_path(const char *&in)
{
	QString *str = eval_quotated_str(in);
	if (str != nullptr)
	{
		auto value = new QString(path_expand(*str));
		delete str;
		str = value;
	}
	return str;
}

/** **********************************************************************************************
 *
 * @brief Read numerical value, decimal, sedecimal or octal
 *
 * @param[out] outval   number
 * @param[in]  minval   minimum valid number
 * @param[in]  maxval   maximum valid number
 * @param[in_out] in    input line pointer, will be advanced accordingly
 *
 * @return TRUE for OK or FALSE for error
 *
 ************************************************************************************************/

bool GuiWindow::eval_int(long &outval, long minval, long maxval, const char *&in)
{
	char *endptr;
	long value;
	
	if (*in == '-')
	{
		value = strtol(in, &endptr, 0 /*auto base*/);
	} else
	{
		value = strtoul(in, &endptr, 0 /*auto base*/);
		if ((unsigned long)value > INT32_MAX)
		{
			value = value - UINT32_MAX - 1;
		}
	}
	if (endptr > in)
	{
		if (maxval > minval && (value < minval || value > maxval))
		{
			// FIXME: use dialog
			qWarning(_("value %ld out of range %ld..%ld"), value, minval, maxval);
			return false;
		}
		outval = value;
		in = endptr;
		return true;
	}
	return false;
}

/** **********************************************************************************************
 *
 * @brief Evaluate boolean string "yes" or "no", optionally enclosed in "" or ''
 *
 * @param[out]    outval   boolean value
 * @param[in_out] in       input line pointer, will be advanced accordingly
 *
 * @return 0 for OK or 1 for error
 *
 ************************************************************************************************/
bool GuiWindow::eval_bool(PrefWidget *w, const char *&line)
{
	QString *YesOrNo;
	bool ok;

	YesOrNo = eval_quotated_str(line);
	ok = YesOrNo != nullptr;
	if (ok)
	{
		w->setPrefValue(*YesOrNo);
	}
	delete YesOrNo;

	return ok;
}

/** **********************************************************************************************
 *
 * @brief Evaluate a single preferences line
 *
 * @param[in]  line   input line, with trailing \n and zero byte
 *
 * @return TRUE for OK, FALSE for error
 *
 * @note empty lines, sections and comments have already been processed
 *
 ************************************************************************************************/

bool GuiWindow::evaluatePreferencesLine(const char *line, bool gui_only)
{
	bool ok = true;
	const char *key;
	QString *str;

	for (auto w: widget_list)
	{
		key = w->name();
		size_t keylen = strlen(key);
		/* FIXME: should use locale-independant function */
		if (strncasecmp(line, key, keylen) == 0 && (ISSPACE(line[keylen]) || line[keylen] == '='))
		{
			if (gui_only != w->gui_only)
			{
				qWarning("%s: expected gui_only=%d, got %d", key, w->gui_only, gui_only);
				return false;
			}
			line += keylen;

			/* skip spaces */
			while (ISSPACE(*line))
			{
				line++;
			}

			if (*line != '=')
			{
				return false;
			}
			line++;

			/* skip spaces */
			while (ISSPACE(*line))
			{
				line++;
			}

			switch (w->type)
			{
			case TYPE_PATH:
			case TYPE_FOLDER:
				{
					PrefPath *p = dynamic_cast<PrefPath *>(w);
					if (p->hasFlags())
					{
						long lv = 0;
						ok = eval_int(lv, 0, 0, line);
						if (!ok)
							break;
						p->setFlags(lv);
						while (ISSPACE(*line))
							line++;
					}
					str = eval_quotated_str_path(line);
					if (str != nullptr)
					{
						w->setPrefValue(*str);
						delete str;
						w->setOrigValue();
					} else
					{
						ok = false;
					}
				}
				break;

			case TYPE_STRING:
				str = eval_quotated_str(line);
				if (str != nullptr)
				{
					w->setPrefValue(*str);
					delete str;
					w->setOrigValue();
				} else
				{
					ok = false;
				}
				break;

			case TYPE_BOOL:
				ok = eval_bool(w, line);
				if (ok)
					w->setOrigValue();
				break;

			case TYPE_INT:
			case TYPE_UINT:
				{
					long lv = 0;
					PrefInt *p = dynamic_cast<PrefInt *>(w);
					ok = eval_int(lv, p->minimum(), p->maximum(), line);
					if (ok)
					{
						w->setPrefValue(QString().setNum(lv));
						w->setOrigValue();
					}
				}
				break;

			case TYPE_CHOICE:
				{
					long lv = 0;
					// PrefChoice *p = dynamic_cast<PrefChoice *>(w);
					ok = eval_int(lv, 0, 0, line);
					if (ok)
					{
						w->setPrefValue(QString().setNum(lv));
						w->setOrigValue();
					}
				}
				break;

			default:
				assert(0);
			}

			if (ok)
			{
				/* skip trailing blanks */
				while (ISSPACE(*line))
				{
					line++;
				}
				if (*line != '\0')
				{
					ok = false;   /* rubbish at end of line */
				}
			}

			return ok;
		}
	}

	qCritical(_("unknown key"));
	return false;
}

/** **********************************************************************************************
 *
 * @brief Read all preferences from a configuration file or write default file, if requested
 *
 * @param[in]  gui    GUI parameters
 *
 * @return number of errors
 *
 ************************************************************************************************/
bool GuiWindow::getPreferences(void)
{
	QFile file(config_file);
	bool ok = true;
	bool nline_ok;
	int lineno = 0;

	if (!file.open(QFile::ReadOnly | QFile::Text))
	{
		/* Configuration file does not exist. Use defaults. */
		// qWarning(_("%s does not exist, using defaults"), config_file.toUtf8().constData());
		return true;
	}
	while (!file.atEnd())
	{
		QByteArray line = file.readLine();
		const char *c = line.constData();
		bool gui_only = false;

		lineno++;

		while (ISSPACE(*c))
		{
			c++;
		}

		/* skip section names */
		if (*c == '[')
		{
			/* TODO: check against known section names? */
			continue;
		}

		/* skip empty lines */
		if (*c == '\0')
		{
			continue;
		}

		/* skip comments */
		if (*c == '#')
		{
			if (c[1] != '.' || ISSPACE(c[2]))
			{
				/* TODO: stash it away so comments can be preserved */
				continue;
			}
			/*
			 * currently only for show_tooltips; maybe for others later
			 */
			c += 2;
			gui_only = true;
		}

		nline_ok = evaluatePreferencesLine(c, gui_only);
		if (!nline_ok)
		{
			/* FIXME: use dialog */
			line.chop(1); // remove trailing newline
			qCritical(_("Syntax error in configuration file:%d: %s"), lineno, line.constData());
		}
		ok &= nline_ok;
	}
	file.close();
	return ok;
}

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

/*
 * XML Parser
 */

/*** ---------------------------------------------------------------------- ***/

void GuiWindow::parseXml(void)
{
	int tab_index;
	const xml_section *section;

	pref_show_tooltips = nullptr;
	language_choice = nullptr;
	
	tab_index = 0;
	for (section = xml_sections; section < &xml_sections[sizeof(xml_sections) / sizeof(xml_sections[0])]; section++)
	{
		int section_row;
		QGridWidget *section_table;
		const xml_widget *widget;
		QWidget *parent;
		
		QString section_name(section->name);
		section_names.push_back(section_name);
		section_table = new QGridWidget();
		
		if (section->scrolled)
		{
			/*
			 * put the path list in a scrolled window, it gets too large
			 */
			auto scroller = new QScrollArea();
			auto vbox = new QVBox();
			scroller->setWidget(section_table);
			vbox->addWidget(scroller);
			scroller->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

			scroller->show();
			parent = vbox;
		} else
		{
			parent = section_table;
		}

		if (section->icon_name == NULL)
			notebook->addTab(parent, _(section->label));
		else
			notebook->addTab(parent, QIcon::fromTheme(section->icon_name), _(section->label));
		tabNames.push_back(U_(section->label));
		tab_index++;
		section_row = 0;
		for (widget = section->widgets; widget->type != TYPE_NONE; widget++)
		{
			switch (widget->type)
			{
			case TYPE_FOLDER:
			case TYPE_PATH:
				{
					auto label = new QLabel();
					section_table->addWidget(label, section_row, 0);
					auto button = new PrefPath(section_name, widget->name, widget->u.path.default_value, widget->gui_only, widget->type == TYPE_FOLDER);
					button->setFlags(widget->u.path.flags);
					button->m_default_flags = widget->u.path.flags;
					button->m_orig_flags = widget->u.path.flags;
					section_table->addWidget(button, section_row, 1);
					button->setToolTip(widget->tooltip);
					button->setButtonLabel(label, widget->label);
					label->setBuddy(button);
					addSetting(button);
					if (strcmp(widget->name, "atari_drv_c") == 0 ||
						strcmp(widget->name, "atari_drv_h") == 0 ||
						strcmp(widget->name, "atari_drv_m") == 0 ||
						strcmp(widget->name, "atari_drv_u") == 0)
						button->setEnabled(false);
					if (button->hasFlags())
					{
						section_row++;
						auto hbox = new QHBox();
			
						button->button_rdonly = new TranslatableCheckBox(N_("read-only"));
						button->button_rdonly->setToolTip(N_("Mount drive in read-only mode, preventing writes from the emulation"));
						hbox->addWidget(button->button_rdonly);
						
						button->button_dosnames = new TranslatableCheckBox(N_("DOS 8+3 format"));
						button->button_dosnames->setToolTip(N_("Force 8.3 short filenames (FAT-style) on host directories"));
						hbox->addWidget(button->button_dosnames);
						
						button->button_insensitive = new TranslatableCheckBox(N_("case insensitive"));
						button->button_insensitive->setToolTip(N_("Ignore case sensitivity when accessing files/folders"));
						hbox->addWidget(button->button_insensitive);
						
						section_table->addWidget(hbox, section_row, 1, 1, 2);
					}
					section_row++;
				}
				break;
			
			case TYPE_STRING:
				{
					auto label = new QLabel();
					section_table->addWidget(label, section_row, 0);
					auto entry = new PrefString(section_name, widget->name, widget->u.string.default_value, widget->gui_only);
					section_table->addWidget(entry, section_row, 1);
					label->setBuddy(entry);
					entry->setToolTip(widget->tooltip);
					entry->setButtonLabel(label, widget->label);
					addSetting(entry);
					section_row++;
				}
				break;
			
			case TYPE_INT:
			case TYPE_UINT:
				{
					auto label = new QLabel();
					section_table->addWidget(label, section_row, 0);
					auto button = new PrefInt(section_name, widget->name, widget->u.integer.default_value, widget->gui_only);
					button->type = widget->type;
					button->setRange(widget->u.integer.minval, widget->u.integer.maxval);
					button->setSingleStep(widget->u.integer.step);
					button->setToolTip(widget->tooltip);
					section_table->addWidget(button, section_row, 1);
					button->setButtonLabel(label, widget->label);
					addSetting(button);
					section_row++;
				}
				break;
			
			case TYPE_BOOL:
				{
					auto button = new PrefBool(section_name, widget->name, widget->label, widget->u.boolvalue.default_value, widget->gui_only);
					button->setToolTip(widget->tooltip);
					section_table->addWidget(button, section_row, 1);
					addSetting(button);
					if (strcmp(widget->name, "show_tooltips") == 0)
					{
						pref_show_tooltips = button;
						connect(pref_show_tooltips, SIGNAL(clicked()), this, SLOT(enableTooltips()));
					}
					section_row++;
				}
				break;

			case TYPE_CHOICE:
				{
					const xml_widget_choice *c;
					auto label = new QLabel();
					section_table->addWidget(label, section_row, 0);
					auto choice = new PrefChoice(section_name, widget->name, widget->u.choice.default_value, widget->gui_only);
					choice->setToolTip(widget->tooltip);
					choice->setButtonLabel(label, widget->label);
					section_table->addWidget(choice, section_row, 1);
					label->setBuddy(choice);
					addSetting(choice);
					section_row++;
					if (strcmp(widget->name, "gui_language") == 0)
					{
						language_choice = choice;
#ifdef ENABLE_NLS
						language_choice->connect(language_choice, SIGNAL(currentIndexChanged(int)), this, SLOT(language_changed(int)));
#else
						language_choice->setEnabled(false);
#endif
					}
					for (c = widget->u.choice.choices; c->label != NULL; c++)
					{
						QIcon icon;
						if (c->icon_name != NULL)
							icon = QIcon(QString(":/icons/") + QString(c->icon_name));
						choice->addItem(c->label, icon, c->value);
					}
				}
				break;
			}
		}

		/*
		 * add a stretchable box to the end of the grid,
		 * so the widgets are placed at the top and not distributed
		 * across the available space
		 */
		auto vbox = new QVBox();
		vbox->addStretch();
		section_table->addWidget(vbox, section_row, 0, 1, 2);
	}
}

/******************************************************************************/
/*** ---------------------------------------------------------------------- ***/
/******************************************************************************/

/*
 * GUI
 */

/** **********************************************************************************************
 *
 * @brief Update current preferences from GUI
 *
 * @param[in] cfgfile  path
 *
 * @return TRUE/FALSE
 *
 ************************************************************************************************/

bool GuiWindow::updatePreferences(void)
{
	for (auto w: widget_list)
	{
		w->updatePreferences();
	}
	return true;
}

/** **********************************************************************************************
 *
 * @brief Populate GUI from current preferences
 *
 * @param[in] cfgfile  path
 *
 * @return true/false
 *
 ************************************************************************************************/
bool GuiWindow::populatePreferences(void)
{
	for (auto w: widget_list)
	{
		w->populatePreferences();
	}

	enableTooltips();

	if (language_choice)
	{
	}
	return true;
}

void GuiWindow::enableTooltips(void)
{
	retranslate_ui(pref_show_tooltips && pref_show_tooltips->isChecked());
}

QString GuiWindow::defaultConfig(void)
{
#if defined(__APPLE__)
	return path_expand("~/Library/Preferences/magic-on-linux.conf");
#elif defined(__HAIKU)
	return path_expand("~/config/settings/magic-on-linux.conf");
#else
	return path_expand("~/.config/magic-on-linux.conf");
#endif

}


GuiWindow::GuiWindow(QWidget *parent) :
	QMainWindow(parent, Qt::Window),
	exit_code(EXIT_WINDOW_CLOSED),
	notebook(nullptr),
	language_choice(nullptr),
	config_file(),
	pref_show_tooltips(nullptr)
{
	qInitResources();

	config_file = defaultConfig();

	auto action = new QAction("close");
	action->setShortcutContext(Qt::WindowShortcut);
	action->setShortcuts(QList<QKeySequence>{ QKeySequence(Qt::Key_Q | Qt::CTRL), QKeySequence(Qt::Key_Escape) });
	this->addAction(action);
	connect(action, SIGNAL(triggered()), this, SLOT(cancel_clicked()));

	setWindowTitle(N_("MagicOnLinux Settings"));
	// resize(800, 600);
	auto centralwidget = new QVBox(this);
	centralwidget->setObjectName("centralwidget");
	notebook = new QTabWidget(centralwidget);
	centralwidget->addWidget(notebook);

	button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, centralwidget);
	ok_button = button_box->button(QDialogButtonBox::Ok);
	connect(ok_button, SIGNAL(clicked()), this, SLOT(ok_clicked()));
	cancel_button = button_box->button(QDialogButtonBox::Cancel);
	connect(cancel_button, SIGNAL(clicked()), this, SLOT(cancel_clicked()));

	centralwidget->addWidget(button_box);
	setCentralWidget(centralwidget);
}

/*** ---------------------------------------------------------------------- ***/

bool GuiWindow::anyChanged(void)
{
	bool changed = false;
	
	for (auto w: widget_list)
	{
		bool this_changed = w->isChanged();
		changed |= this_changed;
	}

	return changed;
}

/*** ---------------------------------------------------------------------- ***/

void GuiWindow::quitApp(void)
{
	QCoreApplication::exit(exit_code);
}

/*** ---------------------------------------------------------------------- ***/

void GuiWindow::ok_clicked(void)
{
	updatePreferences();
	if (writePreferences())
	{
		exit_code = EXIT_SUCCESS;
		quitApp();
	}
}

/*** ---------------------------------------------------------------------- ***/

void GuiWindow::retranslate_ui(bool enableTooltips)
{
	QMainWindow::setWindowTitle(_(m_orig_title));
	for (int i = 0; i < notebook->count(); i++)
	{
#ifdef FORCE_LIBINTL
		notebook->tabBar()->setTabText(i, _(tabNames[i].toUtf8().constData()));
#else
		notebook->tabBar()->setTabText(i, _(tabNames[i]));
#endif
	}
	for (auto w: widget_list)
		w->retranslate_ui(enableTooltips);
	/*
	 * retranslate button_box buttons
	 */
	// does not work:
	// QDialogButtonBox bbox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	// ok_button->setText(bbox.tr("&Ok"));
	// cancel_button->setText(bbox.tr("&Cancel"));
	// does not work either:
	// ok_button->setText(QApplication::tr("&Ok"));
	// cancel_button->setText(QApplication::tr("&Cancel"));
	// ok_button->setText(QApplication::tr("&Ok"));
	// cancel_button->setText(QApplication::tr("&Cancel"));
	// does not work either:
	// QEvent event(QEvent::LanguageChange);
	// QCoreApplication::sendEvent(button_box, &event);
	// QCoreApplication::sendEvent(ok_button, &event);
	// QCoreApplication::sendEvent(cancel_button, &event);
	// crashes with coredump:
	// if (button_box)
	//	button_box->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	if (enableTooltips)
	{
		ok_button->setToolTip(_("Accept and confirm the changes in this dialog"));
		cancel_button->setToolTip(_("Discard the current changes and close the dialog"));
	} else
	{
		ok_button->setToolTip(QString());
		cancel_button->setToolTip(QString());
	}
}

#ifdef ENABLE_NLS
void GuiWindow::language_changed(int index)
{
	const char *lang_name;

	/* The index is -1 if the combobox becomes empty or the currentIndex was reset. */
	if (language_choice && index >= 0)
	{
		language_t lang = (language_t)language_choice->items[index].value;
		if (lang == LANG_SYSTEM)
		{
			setlocale(LC_MESSAGES, "");
			lang = language_get_default();
		}
		{
			lang_name = language_get_name(lang);
			setlocale(LC_MESSAGES, lang_name);
		}
		retranslate_ui(pref_show_tooltips && pref_show_tooltips->isChecked());
		// QEvent event(QEvent::LanguageChange);
		// QApplication::sendEvent(theGui, &event);
	}
}
#endif

/*** ---------------------------------------------------------------------- ***/

void GuiWindow::cancel_clicked(void)
{
	updatePreferences();
	if (anyChanged())
	{
		QMessageBox dialog(QMessageBox::Question,
			_(m_orig_title),
			_("You have unsaved changes. Do you want to discard them and quit?"),
			QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		int response = dialog.exec();
		switch (response)
		{
		case QMessageBox::Save:
			if (!writePreferences())
				return;
			break;
		case QMessageBox::Discard:
			break;
		case QMessageBox::Cancel:
		default:
			return;
		}
	}
	exit_code = EXIT_FAILURE;
	quitApp();
}

/*** ---------------------------------------------------------------------- ***/


int main(int argc, char **argv)
{
	QWidget *parent = 0;

#if defined(NDEBUG)
	// close(2);
#endif

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-session") == 0 || strcmp(argv[i], "--session") == 0)
			return EXIT_SUCCESS;
	}

	setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
	{
		const char *lang_name = language_get_name(language_get_default());

#ifdef FORCE_LIBINTL
		/*
		 * assumes message catalogs to be installed in same directory where Qt was installed, eg. /usr
		 * FIXME: for macOS, use bundle directory?
		 */
#if 0
		static QByteArray locale_dir(QFileInfo(QFileInfo(QFileInfo(QApplication::libraryPaths().first()).dir().path()).path()).dir().path().append("/share/locale").toUtf8());

		bindtextdomain(_STRINGIFY(GETTEXT_PACKAGE), locale_dir.constData());
#else
		const char *dir = (bindtextdomain)("libc", NULL);
		bindtextdomain(_STRINGIFY(GETTEXT_PACKAGE), dir);
#endif
#else
		bindtextdomain(_STRINGIFY(GETTEXT_PACKAGE), NULL);
#endif
		textdomain(_STRINGIFY(GETTEXT_PACKAGE));
		bind_textdomain_codeset(_STRINGIFY(GETTEXT_PACKAGE), "UTF-8");
		setlocale(LC_MESSAGES, lang_name);
	}
#endif
	
#if 0
	QList<QLocale> allLocales = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyCountry);
	for (auto it: allLocales)
	{
		printf("%s, %s, %s, %s\n",
			it.name().toUtf8().constData(),
			QLocale::languageToString(it.language()).toUtf8().constData(),
			QLocale::countryToString(it.country()).toUtf8().constData(),
			it.nativeLanguageName().toUtf8().constData()
			);
	}
	return 0;
#endif

	QCommandLineParser parser;
	QList<QCommandLineOption> options;
	options.append(QCommandLineOption("parent", _("ID of the parent window"), "window-id"));
	options.append(QCommandLineOption("config", _("Specify an alternative configuration file path"), _("FILE")));
	
	parser.addOptions(options);
	parser.addHelpOption();
	parser.addVersionOption();
	
	QApplication app(argc, argv);

	app.setApplicationName(program_name);
	app.setApplicationVersion(program_version);
	app.setQuitOnLastWindowClosed(true);
	
	// parse command line
	parser.process(app);

	if (parser.isSet("parent"))
	{
		WId parent_id = parser.value("parent").toLong(nullptr, 0);
		parent = QWidget::find(parent_id);
	}

	GuiWindow gui(parent);

	if (parser.isSet("config"))
	{
		gui.config_file = parser.value("config");
	}

	gui.parseXml();
	gui.notebook->setCurrentIndex(0);

	gui.getPreferences();
	gui.populatePreferences();

	/* open the window */
	gui.show();
	app.exec();

	return gui.exit_code;
}

#include "mxqt-settings.moc"

#endif /* NLS_TRANSLATE_TR */
