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
#include <QtCore/QXmlStreamReader>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/QAction>
#else
#include <QtWidgets/QAction>
#endif
#include <unistd.h>
#include <assert.h>
#define ENABLE_NLS
#include "mxnls.h"
#include "country.c"
#include "qrc.cc"

#define EXIT_WINDOW_CLOSED (EXIT_FAILURE + EXIT_SUCCESS + 1)

static char const program_name[] = "mxqt-settings";
static char const program_version[] = "1.0";

enum {
	TYPE_NONE,
	TYPE_PATH,
	TYPE_FOLDER,
	TYPE_STRING,
	TYPE_INT,
	TYPE_UINT,
	TYPE_BOOL,
	TYPE_CHOICE
};

#define _STRINGIFY1(x) #x
#define _STRINGIFY(x) _STRINGIFY1(x)

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
		m_orig_text = label;
	}
	void setToolTip(const char *tooltip)
	{
		m_orig_tooltip = tooltip;
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
	QByteArray m_orig_text;
	QByteArray m_orig_tooltip;
};

class Q_WIDGETS_EXPORT PrefWidget
{
public:
	int type;
	bool gui_only;
	QByteArray m_section;
	QByteArray m_name;
	const char *element_name;

	PrefWidget(int type, const char *element_name, QString &section, QString &name, bool gui_only) :
		type(type),
		gui_only(gui_only),
		m_section(section.toUtf8().append('\0')),
		m_name(name.toUtf8().append('\0')),
		element_name(element_name)
	{
		if (name.isEmpty())
		{
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
	QByteArray m_orig_text;
	QByteArray m_orig_tooltip;
public:
	virtual void retranslate_ui(bool enableTooltips) = 0;
};

class Q_WIDGETS_EXPORT PrefBool : public QCheckBox, public PrefWidget
{
public:
	PrefBool(QString &section, QString &name, QString &text, bool default_value, bool gui_only) :
		PrefWidget(TYPE_BOOL, "bool", section, name, gui_only),
		m_value(default_value),
		m_default_value(default_value),
		m_orig_value(default_value)
	{
		m_orig_text = text.toUtf8().append('\0');
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
	void setToolTip(const char *tooltip)
	{
		m_orig_tooltip = tooltip;
		QWidget::setToolTip(_(m_orig_tooltip));
	}
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

/*
 * QSpinBox can only hold values of type int,
 * which is not good enoguh for a max memory_size of 0x80000000
 */
class Q_WIDGETS_EXPORT PrefInt : public QDoubleSpinBox, public PrefWidget
{
public:
	PrefInt(QString &section, QString &name, const QString &default_value, bool gui_only) :
		PrefWidget(TYPE_INT, "int", section, name, gui_only),
		m_value(default_value.toLong()),
		m_default_value(default_value.toLong()),
		m_orig_value(default_value.toLong())
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
	virtual void retranslate_ui(bool enableTooltips)
	{
		button_label->setText(_(m_orig_text));
		if (enableTooltips)
			QWidget::setToolTip(_(m_orig_tooltip));
		else
			QWidget::setToolTip(QString());
	}
public:
	void setButtonLabel(QLabel *label, QString text)
	{
		m_orig_text = text.toUtf8().append('\0');
		button_label = label;
	}
};

class Q_WIDGETS_EXPORT PrefString : public QLineEdit, public PrefWidget
{
public:
	PrefString(QString &section, QString &name, const QString &default_value, bool gui_only) :
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
	virtual void retranslate_ui(bool enableTooltips)
	{
		button_label->setText(_(m_orig_text));
		if (enableTooltips)
			QWidget::setToolTip(_(m_orig_tooltip));
		else
			QWidget::setToolTip(QString());
	}
public:
	void setButtonLabel(QLabel *label, QString text)
	{
		m_orig_text = text.toUtf8().append('\0');
		button_label = label; 
	}
};

class Q_WIDGETS_EXPORT PrefPath : public QPushButton, public PrefWidget
{
	Q_OBJECT

#define NO_FLAGS -1
#define DRV_FLAG_RDONLY         1   /* read-only */
#define DRV_FLAG_8p3            2   /* filenames in 8+3 format, uppercase */
#define DRV_FLAG_CASE_INSENS    4   /* case insensitive, e.g. (V)FAT or HFS(+) */
public:
	PrefPath(QString &section, QString &name, const QString &default_value, bool gui_only, bool isFolder = false) :
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
	void setToolTip(const char *tooltip)
	{
		m_orig_tooltip = tooltip;
		QWidget::setToolTip(_(m_orig_tooltip));
	}
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
	void setButtonLabel(QLabel *label, QString text)
	{
		m_orig_text = text.toUtf8().append('\0');
		button_label = label;
	}
};

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

class Q_WIDGETS_EXPORT PrefChoice : public QComboBox, public PrefWidget
{
	Q_OBJECT

public:
	PrefChoice(QString &section, QString &name, const QString &default_value, bool gui_only) :
		PrefWidget(TYPE_CHOICE, "choice", section, name, gui_only),
		m_value(default_value.toInt()),
		m_default_value(default_value.toInt()),
		m_orig_value(default_value.toInt())
	{
		this->setObjectName(m_name);
	}
	typedef struct { QByteArray orig_text; int value; } choiceItem;
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
	void setToolTip(const char *tooltip)
	{
		m_orig_tooltip = tooltip;
		QWidget::setToolTip(_(m_orig_tooltip));
	}
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
		items.push_back({text, value});
		QComboBox::addItem(icon, _(text));
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
	void setButtonLabel(QLabel *label, QString text)
	{
		m_orig_text = text.toUtf8().append('\0');
		button_label = label;
	}
};

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
	QStringList tabNames;
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
		m_orig_title = title;
		QMainWindow::setWindowTitle(_(m_orig_title));
	}
	void quitApp(void);
protected:
	QByteArray m_orig_title;
	QDialogButtonBox *button_box;
	QPushButton *ok_button;
	QPushButton *cancel_button;
};


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
			qWarning("value %ld out of range %ld..%ld", value, minval, maxval);
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

	qCritical("unknown key\n");
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
		// qWarning("%s does not exist", config_file.toUtf8().constData());
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
class PreferenceParser : public QXmlStreamReader
{
public:
	PreferenceParser(const char *data, GuiWindow &gui);

	void start_element(void);
	void end_element(void);

protected:
	GuiWindow &gui;
	QString section_name;
	QGridWidget *section_table;
	int section_row;
	int tab_index;
	PrefChoice *choice;
};

PreferenceParser::PreferenceParser(const char *data, GuiWindow &gui) :
	QXmlStreamReader(data),
	gui(gui),
	section_name(),
	section_table(nullptr),
	section_row(0),
	tab_index(0),
	choice(nullptr)
{
}

/*** ---------------------------------------------------------------------- ***/

void PreferenceParser::start_element(void)
{
	QString element_name = this->name().toString();
	QString name = this->attributes().value("name").toString();
	QString icon_name = this->attributes().value("icon").toString();
	QString display = this->attributes().value("_label").toUtf8().constData();
	QString tooltip = this->attributes().value("_tooltip").toUtf8().constData();
	QString default_value = this->attributes().value("default").toString();
	bool scrolled = this->attributes().hasAttribute("scrolled");
	bool gui_only = this->attributes().hasAttribute("gui");

	// fprintf(stderr, "start_element: %s %s\n", element_name.toUtf8().constData(), name.toUtf8().constData());
	if (display.isEmpty())
		display = name;
	if (element_name == "preferences")
	{
		; /* ignore */
	} else if (element_name == "section")
	{
		QWidget *parent;

		assert(section_table == nullptr);
		assert(!name.isEmpty());
		section_name = QString(name);
		gui.section_names.push_back(section_name);
		section_row = 0;
		section_table = new QGridWidget();
		if (scrolled)
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
		if (icon_name.isEmpty())
			gui.notebook->addTab(parent, _(display.toUtf8().constData()));
		else
			gui.notebook->addTab(parent, QIcon::fromTheme(icon_name), _(display.toUtf8().constData()));
		gui.tabNames.push_back(display);
		tab_index++;
	} else if (element_name == "folder" || element_name == "path")
	{
		int flags = NO_FLAGS;
		assert(section_table != nullptr);
		auto label = new QLabel(_(display.toUtf8().constData()));
		if (this->attributes().hasAttribute("flags"))
			flags = this->attributes().value("flags").toInt();
		section_table->addWidget(label, section_row, 0);
		auto button = new PrefPath(section_name, name, default_value, gui_only, element_name == "folder");
		button->setFlags(flags);
		button->m_default_flags = flags;
		button->m_orig_flags = flags;
		section_table->addWidget(button, section_row, 1);
		button->setToolTip(tooltip.toUtf8().constData());
		button->setButtonLabel(label, display);
		label->setBuddy(button);
		gui.addSetting(button);
		if (name == "atari_drv_c" ||
			name == "atari_drv_h" ||
			name == "atari_drv_m" ||
			name == "atari_drv_u")
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
	} else if (element_name == "string")
	{
		assert(section_table != nullptr);
		auto label = new QLabel(_(display.toUtf8().constData()));
		section_table->addWidget(label, section_row, 0);
		auto entry = new PrefString(section_name, name, default_value, gui_only);
		section_table->addWidget(entry, section_row, 1);
		label->setBuddy(entry);
		entry->setToolTip(tooltip.toUtf8().constData());
		entry->setButtonLabel(label, display);
		gui.addSetting(entry);
		section_row++;
	} else if (element_name == "int")
	{
		long minval = 0;
		long maxval = INT_MAX; /* LONG_MAX might not be representable in a double */
		long step = 1;

		assert(section_table != nullptr);
		if (this->attributes().hasAttribute("minval"))
			minval = this->attributes().value("minval").toLong();
		if (this->attributes().hasAttribute("maxval"))
			maxval = this->attributes().value("maxval").toLong();
		if (this->attributes().hasAttribute("step"))
			step = this->attributes().value("step").toLong();
		auto label = new QLabel(_(display.toUtf8().constData()));
		section_table->addWidget(label, section_row, 0);
		auto button = new PrefInt(section_name, name, default_value, gui_only);
		button->setRange(minval, maxval);
		button->setSingleStep(step);
		button->setToolTip(tooltip.toUtf8().constData());
		section_table->addWidget(button, section_row, 1);
		button->setButtonLabel(label, display);
		gui.addSetting(button);
		/*
		 * some values must be written as unsigned, or the application fails to parse them
		 */
		if (name.startsWith("app_window_"))
			button->type = TYPE_UINT;
		section_row++;
	} else if (element_name == "bool")
	{
		assert(section_table != nullptr);
		auto button = new PrefBool(section_name, name, display, bool_from_string(default_value), gui_only);
		button->setToolTip(tooltip.toUtf8().constData());
		section_table->addWidget(button, section_row, 1);
		gui.addSetting(button);
		if (name == "show_tooltips")
			gui.pref_show_tooltips = button;
		section_row++;
	} else if (element_name == "choice")
	{
		assert(section_table != nullptr);
		assert(choice == nullptr);
		auto label = new QLabel(_(display.toUtf8().constData()));
		section_table->addWidget(label, section_row, 0);
		choice = new PrefChoice(section_name, name, default_value, gui_only);
		choice->setToolTip(tooltip.toUtf8().constData());
		choice->setButtonLabel(label, display);
		section_table->addWidget(choice, section_row, 1);
		label->setBuddy(choice);
		gui.addSetting(choice);
		section_row++;
		if (name == "gui_language")
		{
			gui.language_choice = choice;
#ifdef ENABLE_NLS
			gui.language_choice->connect(gui.language_choice, SIGNAL(currentIndexChanged(int)), &gui, SLOT(language_changed(int)));
#else
			gui.language_choice->setEnabled(false);
#endif
		}
	} else if (element_name == "select")
	{
		int value;

		assert(section_table != nullptr);
		assert(choice != nullptr);
		if (this->attributes().hasAttribute("value"))
			value = this->attributes().value("value").toInt();
		else
			value = choice->count();
		QIcon icon;
		if (this->attributes().hasAttribute("icon"))
			icon = QIcon(QString(":/icons/") + this->attributes().value("icon").toString());
		choice->addItem(display.toUtf8().constData(), icon, value);
	} else
	{
		/* FIXME: should be fatal error */
		qWarning("unsupported element %s\n", element_name.toUtf8().constData());
	}
}

/*** ---------------------------------------------------------------------- ***/

void PreferenceParser::end_element(void)
{
	QString element_name = this->name().toString();
	// fprintf(stderr, "end_element: %s\n", element_name.toUtf8().constData());
	if (element_name == "preferences")
	{
		assert(section_table == nullptr);
	} else if (element_name == "section")
	{
		assert(section_table != nullptr);
		// fprintf(stderr, "%s rows: %d\n", section_name.toUtf8().constData(), section_row);
		/*
		 * add a stretchable box to the end of the grid,
		 * so the widgets are placed at the top and not distributed
		 * across the available space
		 */
		auto vbox = new QVBox();
		vbox->addStretch();
		section_table->addWidget(vbox, section_row, 0, 1, 2);
		section_table = nullptr;
		section_name.clear();
	} else if (element_name == "choice")
	{
		assert(choice != nullptr);
		choice->minval = 0;
		choice->maxval = choice->count() - 1;
		choice = nullptr;
	}
}

/*** ---------------------------------------------------------------------- ***/

void GuiWindow::parseXml(void)
{
	/*
	 * XML for the parser.
	 */
	static char const preferences[] =
#include "preferences.xml.h"
	;

	pref_show_tooltips = nullptr;

	PreferenceParser xml(preferences, *this);
	
	while (!xml.atEnd())
	{
		switch (xml.readNext())
		{
		case QXmlStreamReader::NoToken:
		case QXmlStreamReader::StartDocument:
		case QXmlStreamReader::EndDocument:
		case QXmlStreamReader::Comment:
		case QXmlStreamReader::Invalid:
		case QXmlStreamReader::DTD:
		case QXmlStreamReader::EntityReference:
		case QXmlStreamReader::ProcessingInstruction:
			break;

		case QXmlStreamReader::StartElement:
			xml.start_element();
			break;

		case QXmlStreamReader::EndElement:
			xml.end_element();
			break;
		
		case QXmlStreamReader::Characters:
			break;
		}
	}
	
	if (pref_show_tooltips)
	{
		connect(pref_show_tooltips, SIGNAL(clicked()), this, SLOT(enableTooltips()));
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
	return path_expand("~/Library/Preferences/magiclinux.conf");
#else
	return path_expand("~/.config/magiclinux.conf");
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
		notebook->tabBar()->setTabText(i, _(tabNames[i].toUtf8().constData()));
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
