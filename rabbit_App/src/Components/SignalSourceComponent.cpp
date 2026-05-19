#include <QHeaderView>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <cstdint>

#include "Components/AbstractComponent.h"
#include "Components/ComponentSettingsDialog.h"
#include "Components/SignalSourceComponent.h"

using namespace rabbit_App::component;

// ------------------------------------------------------------------
// SignalSourceRawComponent
// ------------------------------------------------------------------

SignalSourceRawComponent::SignalSourceRawComponent(QWidget *parent)
    : AbstractRawComponent(parent) {
  initPorts();
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  elapsed_timer_.start();
}

SignalSourceRawComponent::~SignalSourceRawComponent() {}

void SignalSourceRawComponent::reset() {
  elapsed_timer_.restart();
  update();
}

void SignalSourceRawComponent::processReadData(QQueue<uint64_t> &read_queue) {}

uint64_t SignalSourceRawComponent::getWriteData() const {
  if (input_ports_.isEmpty() || input_ports_[0].pin_index < 0) {
    return 0;
  }
  bool output = computeOutput();
  return (uint64_t)(output ^ is_low_active_) << input_ports_[0].pin_index;
}

bool SignalSourceRawComponent::computeOutput() const {
  switch (mode_) {
  case SignalMode::ConstantLow:
    return false;
  case SignalMode::ConstantHigh:
    return true;
  case SignalMode::SquareWave: {
    int period_cycles = period_ms_; // stored directly in cycles
    if (period_cycles <= 0) return false;
    qint64 elapsed_cycles = elapsed_timer_.elapsed() * frequency_ / 1000;
    qint64 phase = elapsed_cycles % period_cycles;
    return phase < (qint64)(period_cycles * duty_cycle_percent_ / 100);
  }
  case SignalMode::SinglePulse: {
    qint64 elapsed_cycles = elapsed_timer_.elapsed() * frequency_ / 1000;
    int delay_cycles = delay_ms_;    // stored directly in cycles
    int width_cycles = pulse_width_ms_; // stored directly in cycles
    return (elapsed_cycles >= delay_cycles &&
            elapsed_cycles < (qint64)(delay_cycles + width_cycles));
  }
  case SignalMode::Sequence: {
    if (sequence_.isEmpty()) return false;
    qint64 elapsed_cycles = elapsed_timer_.elapsed() * frequency_ / 1000;

    // Loop handling: total time stored directly in cycles
    if (sequence_loop_ && sequence_.size() > 1) {
      int total_period_cycles = sequence_.last().time_ms;
      if (total_period_cycles > 0) {
        elapsed_cycles = elapsed_cycles % total_period_cycles;
      }
    }

    // Find the last entry whose time (in cycles) <= elapsed_cycles
    bool output = sequence_hold_ ? false : sequence_.last().value;
    for (const auto &entry : sequence_) {
      if (entry.time_ms <= elapsed_cycles) {
        output = entry.value;
      } else {
        break;
      }
    }
    return output;
  }
  }
  return false;
}

QString SignalSourceRawComponent::modeString() const {
  switch (mode_) {
  case SignalMode::ConstantLow:
    return tr("Low");
  case SignalMode::ConstantHigh:
    return tr("High");
  case SignalMode::SquareWave:
    return tr("Square");
  case SignalMode::SinglePulse:
    return tr("Pulse");
  case SignalMode::Sequence:
    return tr("Seq");
  }
  return tr("Unknown");
}

void SignalSourceRawComponent::drawSequencePreview(QPainter &painter,
                                                   const QRect &rect) const {
  if (sequence_.isEmpty()) return;

  int max_time = sequence_.last().time_ms;
  if (max_time <= 0) return;

  int display_window = qMin(max_time, 500);
  int segments = rect.width();
  if (segments <= 0) return;

  for (int i = 0; i < segments; ++i) {
    int t = i * display_window / segments;
    bool val = false;
    for (const auto &entry : sequence_) {
      if (entry.time_ms <= t)
        val = entry.value;
      else
        break;
    }
    QColor c = val ? QColor(34, 197, 94) : QColor(107, 114, 128);
    painter.setPen(Qt::NoPen);
    painter.setBrush(c);
    painter.drawRect(rect.left() + i, rect.bottom() - 8, 1, 6);
  }
}

void SignalSourceRawComponent::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  bool output = computeOutput();
  QRect rect = this->rect().adjusted(4, 4, -4, -4);

  // Background color based on output state
  QColor bg_color = output ? QColor(34, 197, 94) : QColor(156, 163, 175);
  painter.setBrush(bg_color);
  painter.setPen(Qt::NoPen);
  painter.drawRoundedRect(rect, 6, 6);

  // Border
  painter.setPen(QPen(QColor(75, 85, 99), 2));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(rect, 6, 6);

  // Text: mode name
  painter.setPen(Qt::white);
  QFont font = painter.font();
  font.setPointSize(9);
  font.setBold(true);
  painter.setFont(font);
  painter.drawText(rect.adjusted(0, -6, 0, 0), Qt::AlignCenter, modeString());

  // Sequence preview or indicator dot
  if (mode_ == SignalMode::Sequence) {
    drawSequencePreview(painter, rect.adjusted(2, 0, -2, -4));
  } else {
    QColor dot_color = output ? QColor(220, 252, 231) : QColor(229, 231, 235);
    painter.setBrush(dot_color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(rect.right() - 10, rect.top() + 4, 6, 6);
  }
}

void SignalSourceRawComponent::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    if (mode_ == SignalMode::ConstantLow) {
      mode_ = SignalMode::ConstantHigh;
    } else if (mode_ == SignalMode::ConstantHigh) {
      mode_ = SignalMode::ConstantLow;
    }
    if (mode_ == SignalMode::Sequence) {
      elapsed_timer_.restart();
    }
    update();
  }
}

void SignalSourceRawComponent::initPorts() {
  appendPort(input_ports_, "SIG", ports::PortType::Input);
}

QString SignalSourceRawComponent::sequenceToString() const {
  QStringList parts;
  for (const auto &entry : sequence_) {
    parts.append(QString::number(entry.time_ms) + ":" +
                 (entry.value ? "1" : "0"));
  }
  return parts.join(";");
}

void SignalSourceRawComponent::sequenceFromString(const QString &str) {
  sequence_.clear();
  if (str.isEmpty()) return;
  QStringList entries = str.split(";", Qt::SkipEmptyParts);
  for (const QString &entry : entries) {
    QStringList pair = entry.split(":");
    if (pair.size() == 2) {
      SequenceEntry e;
      e.time_ms = pair[0].toInt();
      e.value = (pair[1].toInt() != 0);
      sequence_.append(e);
    }
  }
  std::sort(sequence_.begin(), sequence_.end(),
            [](const SequenceEntry &a, const SequenceEntry &b) {
              return a.time_ms < b.time_ms;
            });
}

// ------------------------------------------------------------------
// SignalSourceComponent (macro generated boilerplate)
// ------------------------------------------------------------------

COMPONENT_CLASS_DEFINITION(SignalSource, 2, 2)

void SignalSourceComponent::onSettingsBtnClicked() {
  auto raw = qobject_cast<SignalSourceRawComponent *>(raw_component_);
  auto dialog = new SignalSourceSettingsDialog(raw, this, this);
  dialog->exec();
  delete dialog;
}

// ------------------------------------------------------------------
// SignalSourceSettingsDialog
// ------------------------------------------------------------------

SignalSourceSettingsDialog::SignalSourceSettingsDialog(
    SignalSourceRawComponent *raw_component, AbstractComponent *component,
    QWidget *parent)
    : ComponentSettingsDialog(component, parent),
      raw_component_(raw_component) {
  initSignalSettingsUi();
}

SignalSourceSettingsDialog::~SignalSourceSettingsDialog() {}

void SignalSourceSettingsDialog::initSignalSettingsUi() {
  // Mode selection
  auto mode_layout = new QHBoxLayout();
  mode_layout->addWidget(new QLabel(tr("Signal Mode:"), this));
  mode_combo_ = new QComboBox(this);
  mode_combo_->addItem(tr("Constant Low"),
                       static_cast<int>(SignalMode::ConstantLow));
  mode_combo_->addItem(tr("Constant High"),
                       static_cast<int>(SignalMode::ConstantHigh));
  mode_combo_->addItem(tr("Square Wave"),
                       static_cast<int>(SignalMode::SquareWave));
  mode_combo_->addItem(tr("Single Pulse"),
                       static_cast<int>(SignalMode::SinglePulse));
  mode_combo_->addItem(tr("Sequence"),
                       static_cast<int>(SignalMode::Sequence));
  mode_combo_->setCurrentIndex(static_cast<int>(raw_component_->signalMode()));
  mode_layout->addWidget(mode_combo_);
  appendSettingLayout(mode_layout);

  // Period row
  auto period_widget = new QWidget(this);
  auto period_layout = new QHBoxLayout(period_widget);
  period_layout->setContentsMargins(0, 0, 0, 0);
  period_layout->addWidget(new QLabel(tr("Period (cycles):"), this));
  period_edit_ = new QLineEdit(this);
  period_edit_->setValidator(new QIntValidator(1, 999999, this));
  period_edit_->setText(QString::number(raw_component_->periodCycles()));
  period_layout->addWidget(period_edit_);
  appendSettingWidget(period_widget);

  // Duty cycle row
  auto duty_widget = new QWidget(this);
  auto duty_layout = new QHBoxLayout(duty_widget);
  duty_layout->setContentsMargins(0, 0, 0, 0);
  duty_layout->addWidget(new QLabel(tr("Duty Cycle (%):"), this));
  duty_cycle_edit_ = new QLineEdit(this);
  duty_cycle_edit_->setValidator(new QIntValidator(0, 100, this));
  duty_cycle_edit_->setText(
      QString::number(raw_component_->dutyCyclePercent()));
  duty_layout->addWidget(duty_cycle_edit_);
  appendSettingWidget(duty_widget);

  // Delay row
  auto delay_widget = new QWidget(this);
  auto delay_layout = new QHBoxLayout(delay_widget);
  delay_layout->setContentsMargins(0, 0, 0, 0);
  delay_layout->addWidget(new QLabel(tr("Delay (cycles):"), this));
  delay_edit_ = new QLineEdit(this);
  delay_edit_->setValidator(new QIntValidator(0, 999999, this));
  delay_edit_->setText(QString::number(raw_component_->delayCycles()));
  delay_layout->addWidget(delay_edit_);
  appendSettingWidget(delay_widget);

  // Pulse width row
  auto width_widget = new QWidget(this);
  auto width_layout = new QHBoxLayout(width_widget);
  width_layout->setContentsMargins(0, 0, 0, 0);
  width_layout->addWidget(new QLabel(tr("Pulse Width (cycles):"), this));
  pulse_width_edit_ = new QLineEdit(this);
  pulse_width_edit_->setValidator(new QIntValidator(1, 999999, this));
  pulse_width_edit_->setText(
      QString::number(raw_component_->pulseWidthCycles()));
  width_layout->addWidget(pulse_width_edit_);
  appendSettingWidget(width_widget);

  // Sequence table (3 columns: Time + Value)
  auto seq_group = new QGroupBox(tr("Sequence Editor (cycles)"), this);
  auto seq_main_layout = new QVBoxLayout(seq_group);

  sequence_table_ = new QTableWidget(0, 2, this);
  sequence_table_->setHorizontalHeaderLabels(
      QStringList() << tr("Time (cycles)") << tr("Value (0/1)"));
  sequence_table_->horizontalHeader()->setStretchLastSection(true);
  sequence_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  refreshSequenceTable();
  seq_main_layout->addWidget(sequence_table_);

  auto seq_btn_layout = new QHBoxLayout();
  add_row_btn_ = new QPushButton(tr("Add Row"), this);
  del_row_btn_ = new QPushButton(tr("Delete Row"), this);
  seq_btn_layout->addWidget(add_row_btn_);
  seq_btn_layout->addWidget(del_row_btn_);
  seq_btn_layout->addStretch();
  seq_main_layout->addLayout(seq_btn_layout);

  auto seq_opt_layout = new QHBoxLayout();
  loop_check_ = new QCheckBox(tr("Loop sequence"), this);
  loop_check_->setChecked(raw_component_->sequenceLoop());
  hold_check_ = new QCheckBox(tr("Hold last value after finish"), this);
  hold_check_->setChecked(raw_component_->sequenceHold());
  seq_opt_layout->addWidget(loop_check_);
  seq_opt_layout->addWidget(hold_check_);
  seq_opt_layout->addStretch();
  seq_main_layout->addLayout(seq_opt_layout);

  appendSettingWidget(seq_group);

  // Active mode
  auto active_mode_layout = new QHBoxLayout();
  active_high_radio_ = new QRadioButton(tr("High Active"), this);
  active_low_radio_ = new QRadioButton(tr("Low Active"), this);
  active_high_radio_->setChecked(!raw_component_->isLowActive());
  active_low_radio_->setChecked(raw_component_->isLowActive());
  active_mode_layout->addWidget(active_high_radio_);
  active_mode_layout->addWidget(active_low_radio_);
  appendSettingLayout(active_mode_layout);

  connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &SignalSourceSettingsDialog::onModeChanged);
  connect(add_row_btn_, &QPushButton::clicked, this,
          &SignalSourceSettingsDialog::onAddRow);
  connect(del_row_btn_, &QPushButton::clicked, this,
          &SignalSourceSettingsDialog::onDeleteRow);
  onModeChanged(mode_combo_->currentIndex());
}

void SignalSourceSettingsDialog::refreshSequenceTable() {
  const auto &seq = raw_component_->sequence();
  sequence_table_->setRowCount(seq.size());
  for (int i = 0; i < seq.size(); ++i) {
    // time_ms now stores cycles directly
    sequence_table_->setItem(
        i, 0, new QTableWidgetItem(QString::number(seq[i].time_ms)));
    sequence_table_->setItem(
        i, 1, new QTableWidgetItem(seq[i].value ? "1" : "0"));
  }
}

void SignalSourceSettingsDialog::onModeChanged(int index) {
  (void)index;
  auto mode = static_cast<SignalMode>(mode_combo_->currentData().toInt());
  bool show_square = (mode == SignalMode::SquareWave);
  bool show_pulse = (mode == SignalMode::SinglePulse);
  bool show_sequence = (mode == SignalMode::Sequence);

  period_edit_->parentWidget()->setEnabled(show_square);
  duty_cycle_edit_->parentWidget()->setEnabled(show_square);
  delay_edit_->parentWidget()->setEnabled(show_pulse);
  pulse_width_edit_->parentWidget()->setEnabled(show_pulse);
  sequence_table_->parentWidget()->setEnabled(show_sequence);
}

void SignalSourceSettingsDialog::onAddRow() {
  int row = sequence_table_->rowCount();
  sequence_table_->insertRow(row);
  int last_cycles = 0;
  if (row > 0) {
    last_cycles = sequence_table_->item(row - 1, 0)->text().toInt();
  }
  int step = raw_component_->frequency() > 0 ? raw_component_->frequency() / 10 : 100;
  if (step < 1) step = 1;
  sequence_table_->setItem(
      row, 0, new QTableWidgetItem(QString::number(last_cycles + step)));
  sequence_table_->setItem(row, 1, new QTableWidgetItem(QString("0")));
}

void SignalSourceSettingsDialog::onDeleteRow() {
  auto selected = sequence_table_->selectionModel()->selectedRows();
  QList<int> rows;
  for (const auto &idx : selected) {
    rows.append(idx.row());
  }
  std::sort(rows.begin(), rows.end(), std::greater<int>());
  for (int row : rows) {
    sequence_table_->removeRow(row);
  }
}

void SignalSourceSettingsDialog::accept() {
  // Save signal mode
  auto new_mode = static_cast<SignalMode>(mode_combo_->currentData().toInt());
  if (new_mode != raw_component_->signalMode()) {
    raw_component_->setSignalMode(new_mode);
    is_modifieds_ = true;
  }

  // Save parameters (all stored directly in cycles)
  int new_period = period_edit_->text().toInt();
  if (new_period > 0 && new_period != raw_component_->periodCycles()) {
    raw_component_->setPeriodCycles(new_period);
    is_modifieds_ = true;
  }
  int new_duty = duty_cycle_edit_->text().toInt();
  if (new_duty >= 0 && new_duty <= 100 &&
      new_duty != raw_component_->dutyCyclePercent()) {
    raw_component_->setDutyCyclePercent(new_duty);
    is_modifieds_ = true;
  }
  int new_delay = delay_edit_->text().toInt();
  if (new_delay >= 0 && new_delay != raw_component_->delayCycles()) {
    raw_component_->setDelayCycles(new_delay);
    is_modifieds_ = true;
  }
  int new_width = pulse_width_edit_->text().toInt();
  if (new_width > 0 && new_width != raw_component_->pulseWidthCycles()) {
    raw_component_->setPulseWidthCycles(new_width);
    is_modifieds_ = true;
  }

  // Save sequence (stored directly in cycles, no conversion)
  QVector<SequenceEntry> new_sequence;
  for (int i = 0; i < sequence_table_->rowCount(); ++i) {
    SequenceEntry e;
    e.time_ms = sequence_table_->item(i, 0)->text().toInt(); // cycles
    e.value = sequence_table_->item(i, 1)->text().toInt() != 0;
    new_sequence.append(e);
  }
  std::sort(new_sequence.begin(), new_sequence.end(),
            [](const SequenceEntry &a, const SequenceEntry &b) {
              return a.time_ms < b.time_ms;
            });
  if (new_sequence != raw_component_->sequence()) {
    raw_component_->setSequence(new_sequence);
    is_modifieds_ = true;
  }
  if (loop_check_->isChecked() != raw_component_->sequenceLoop()) {
    raw_component_->setSequenceLoop(loop_check_->isChecked());
    is_modifieds_ = true;
  }
  if (hold_check_->isChecked() != raw_component_->sequenceHold()) {
    raw_component_->setSequenceHold(hold_check_->isChecked());
    is_modifieds_ = true;
  }

  // Save active mode
  bool new_low_active = active_low_radio_->isChecked();
  if (new_low_active != raw_component_->isLowActive()) {
    raw_component_->setLowActive(new_low_active);
    is_modifieds_ = true;
  }

  if (is_modifieds_) {
    raw_component_->update();
  }

  ComponentSettingsDialog::accept();
}
