#include "obs/dock.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include "obs/controller.hpp"

namespace sg {

static bool addAudioSource(void *list, obs_source_t *source)
{
	if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO)
		static_cast<QStringList *>(list)->append(QString::fromUtf8(obs_source_get_name(source)));
	return true;
}

class Dock : public QWidget {
public:
	explicit Dock(Controller *controller) : controller_(controller)
	{
		status_ = new QLabel;
		status_->setAlignment(Qt::AlignCenter);
		status_->setWordWrap(true);
		status_->setMinimumHeight(60);
		reason_ = wrapped();
		lastCheck_ = wrapped();
		lastMatch_ = wrapped();
		enabled_ = new QCheckBox("Guard on");
		source_ = new QComboBox;
		mode_ = new QComboBox;
		mode_->addItem("Mute");
		mode_->addItem("Mute + stop stream");
		unmuteAfter_ = new QSpinBox;
		unmuteAfter_->setRange(30, 600);
		unmuteAfter_->setSuffix(" s");
		pause_ = new QPushButton("Pause");
		unmute_ = new QPushButton("Unmute now");
		events_ = new QListWidget;
		events_->setWordWrap(true);

		auto *form = new QFormLayout;
		form->addRow("Source", source_);
		form->addRow("On a match", mode_);
		form->addRow("Unmute after", unmuteAfter_);
		form->addRow("Last check", lastCheck_);
		form->addRow("Last match", lastMatch_);

		auto *layout = new QVBoxLayout(this);
		layout->addWidget(status_);
		layout->addWidget(reason_);
		layout->addWidget(enabled_);
		layout->addLayout(form);
		layout->addWidget(pause_);
		layout->addWidget(unmute_);
		layout->addWidget(events_, 1);

		connect(enabled_, &QCheckBox::clicked, this, [this]() { save(); });
		connect(source_, &QComboBox::activated, this, [this]() { save(); });
		connect(mode_, &QComboBox::activated, this, [this]() { save(); });
		connect(unmuteAfter_, &QSpinBox::editingFinished, this, [this]() { save(); });
		connect(pause_, &QPushButton::clicked, this,
			[this]() { controller_->setPaused(controller_->guard().state() != State::Paused); });
		connect(unmute_, &QPushButton::clicked, this, [this]() { controller_->unmuteNow(); });

		auto *timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, [this]() { refresh(); });
		timer->start(500);
		refresh();
	}

private:
	static QLabel *wrapped()
	{
		auto *label = new QLabel;
		label->setWordWrap(true);
		return label;
	}

	void save()
	{
		Settings next = controller_->settings();
		next.enabled = enabled_->isChecked();
		if (!source_->currentText().isEmpty())
			next.source = source_->currentText().toStdString();
		next.stopOnMatch = mode_->currentIndex() == 1;
		next.unmuteAfter = unmuteAfter_->value();
		controller_->apply(next);
		refresh();
	}

	void refreshSources(const QString &wanted)
	{
		if (source_->view()->isVisible())
			return;
		QStringList names;
		obs_enum_sources(addAudioSource, &names);
		// Keep the saved name in the list even while that source is missing.
		if (!names.contains(wanted))
			names.prepend(wanted);
		QStringList shown;
		for (int i = 0; i < source_->count(); i++)
			shown.append(source_->itemText(i));
		const QSignalBlocker block(source_);
		if (names != shown) {
			source_->clear();
			source_->addItems(names);
		}
		source_->setCurrentText(wanted);
	}

	void refresh()
	{
		const Settings &settings = controller_->settings();
		const State state = controller_->guard().state();
		flash_ = !flash_;

		const char *text = "";
		const char *color = "#555555";
		switch (state) {
		case State::Off:
			text = "OFF";
			break;
		case State::Blocked:
			text = "NOT ARMED";
			color = "#b35c00";
			break;
		case State::Guarding:
			text = "GUARDING";
			color = "#1e7d32";
			break;
		case State::MutedMatch:
			text = "STREAM MUTED: SONG";
			color = "#b71c1c";
			break;
		case State::MutedFailure:
			text = "STREAM MUTED: CHECKS FAILING";
			color = flash_ ? "#ff1744" : "#4a0000";
			break;
		case State::Paused:
			text = "PAUSED: NOT CHECKING";
			color = flash_ ? "#c79100" : "#6d4f00";
			break;
		case State::Stopped:
			text = "STREAM STOPPED: SONG";
			color = "#b71c1c";
			break;
		}
		status_->setText(text);
		status_->setStyleSheet(
			QString("background:%1; color:white; font-weight:bold; border-radius:4px; padding:6px;")
				.arg(color));
		reason_->setText(state == State::Blocked ? QString::fromStdString(controller_->guard().reason()) : "");
		reason_->setVisible(!reason_->text().isEmpty());
		lastCheck_->setText(QString::fromStdString(controller_->lastCheck()));
		lastMatch_->setText(QString::fromStdString(controller_->lastMatch()));

		enabled_->setChecked(settings.enabled);
		refreshSources(QString::fromStdString(settings.source));
		if (!mode_->view()->isVisible())
			mode_->setCurrentIndex(settings.stopOnMatch ? 1 : 0);
		if (!unmuteAfter_->hasFocus()) {
			const QSignalBlocker block(unmuteAfter_);
			unmuteAfter_->setValue(settings.unmuteAfter);
		}
		pause_->setText(state == State::Paused ? "Resume" : "Pause");
		pause_->setEnabled(state != State::Off && state != State::Blocked);
		unmute_->setEnabled(controller_->guard().muted());

		const auto &events = controller_->events();
		const QString newest = events.empty() ? QString() : QString::fromStdString(events.front());
		if (newest != newestShown_) {
			newestShown_ = newest;
			events_->clear();
			for (const auto &line : events)
				events_->addItem(QString::fromStdString(line));
		}
	}

	Controller *controller_;
	QLabel *status_, *reason_, *lastCheck_, *lastMatch_;
	QCheckBox *enabled_;
	QComboBox *source_, *mode_;
	QSpinBox *unmuteAfter_;
	QPushButton *pause_, *unmute_;
	QListWidget *events_;
	QString newestShown_;
	bool flash_ = false;
};

QWidget *createDock(Controller *controller)
{
	return new Dock(controller);
}

} // namespace sg
