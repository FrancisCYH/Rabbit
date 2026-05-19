#include <QHeaderView>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <cstdint>

#include "Components/AbstractComponent.h"
#include "Components/ComponentSettingsDialog.h"
#include "Components/SignalSource8Component.h"

using namespace rabbit_App::component;

// ------------------------------------------------------------------
// SignalSource8RawComponent
// ------------------------------------------------------------------

SignalSource8RawComponent::SignalSource8RawComponent(QWidget *parent)
    : AbstractRawComponent(parent) {
  initPorts();
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  elapsed_timer_.start();
}

SignalSource8RawComponent::~SignalSource8RawComponent() {}

void SignalSource8RawComponent::reset() {
  elapsed_timer_.restart();
  update();
}

void SignalSource8RawComponent::processReadData(QQueue<uint64_t> &read_queue) {}

uint64_t SignalSource8RawComponent::getWriteData() const {
  uint64_t data = 0;
  uint8_t outputs = computeOutput();
  for (int i = 0; i < input_ports_.size(); ++i) {
    if (input_ports_[i].pin_index >= 0) {
      bool val = (outputs >> i) & 1;
      data |= (uint64_t)(val ^ is_low_active_) << input_ports_[i].pin_index;
    }
  }
  return data;
}

uint8_t SignalSource8RawComponent::computeOutput() const {
  if (sequence_.isEmpty()) return 0x00;
  qint64 elapsed_cycles = elapsed_timer_.elapsed() * frequency_ / 1000;

  // Loop handling: time_ms stores cycles directly
  if (sequence_loop_ && sequence_.size() > 1) {
    int total_period_cycles = sequence_.last().time_ms;
    if (total_period_cycles > 0) {
      elapsed_cycles = elapsed_cycles % total_period_cycles;
    }
  }

  // Find the last entry whose time (in cycles) <= elapsed_cycles
  uint8_t output = sequence_hold_ ? 0x00 : sequence_.last().values;
  for (const auto &entry : sequence_) {
    if (entry.time_ms <= elapsed_cycles) {
      output = entry.values;
    } else {
      break;
    }
  }
  return output;
}

void SignalSource8RawComponent::drawSequencePreview(QPainter &painter,
                                                    const QRect &rect) const {
  if (sequence_.isEmpty()) return;

  int max_time = sequence_.last().time_ms;
  if (max_time <= 0) return;

  int display_window = qMin(max_time, 500);
  int segments = rect.width();
  if (segments <= 0) return;

  int strip_height = 2;
  int gap = 1;
  int ports_to_show = qMin(8, 4);

  for (int port = 0; port < ports_to_show; ++port) {
    int y = rect.bottom() - (ports_to_show - port) * (strip_height + gap);
    for (int i = 0; i < segments; ++i) {
      int t = i * display_window / segments;
      bool val = false;
      for (const auto &entry : sequence_) {
        if (entry.time_ms <= t)
          val = (entry.values >> port) & 1;
        else
          break;
      }
      QColor c = val ? QColor(34, 197, 94) : QColor(107, 114, 128);
      painter.setPen(Qt::NoPen);
      painter.setBrush(c);
      painter.drawRect(rect.left() + i, y, 1, strip_height);
    }
  }

  if (input_ports_.size() > 4) {
    painter.setPen(QColor(209, 213, 219));
    QFont font = painter.font();
    font.setPointSize(6);
    painter.setFont(font);
    painter.drawText(rect.right() - 14, rect.bottom() - 10, 12, 8,
                     Qt::AlignRight, "+" + QString::number(input_ports_.size() - 4));
  }
}

void SignalSource8RawComponent::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  uint8_t outputs = computeOutput();
  bool any_high = (outputs != 0x00);

  QRect rect = this->rect().adjusted(4, 4, -4, -4);

  QColor bg_color = any_high ? QColor(34, 197, 94) : QColor(156, 163, 175);
  painter.setBrush(bg_color);
  painter.setPen(Qt::NoPen);
  painter.drawRoundedRect(rect, 6, 6);

  painter.setPen(QPen(QColor(75, 85, 99), 2));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(rect, 6, 6);

  painter.setPen(Qt::white);
  QFont font = painter.font();
  font.setPointSize(9);
  font.setBold(true);
  painter.setFont(font);
  painter.drawText(rect.adjusted(0, -6, 0, 0), Qt::AlignCenter, tr("Seq x8"));

  drawSequencePreview(painter, rect.adjusted(2, 0, -2, -4));
}

void SignalSource8RawComponent::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    elapsed_timer_.restart();
    update();
  }
}

void SignalSource8RawComponent::initPorts() {
  for (int i = 0; i < 8; ++i) {
    appendPort(input_ports_, QString("SIG%1").arg(i), ports::PortType::Input);
  }
}

QString SignalSource8RawComponent::sequenceToString() const {
  QStringList parts;
  for (const auto &entry : sequence_) {
    parts.append(QString::number(entry.time_ms) + ":" +
                 QString::number(entry.values));
  }
  return parts.join(";");
}

void SignalSource8RawComponent::sequenceFromString(const QString &str) {
  sequence_.clear();
  if (str.isEmpty()) return;
  QStringList entries = str.split(";", Qt::SkipEmptyParts);
  for (const QString &entry : entries) {
    QStringList pair = entry.split(":");
    if (pair.size() == 2) {
      SequenceEntry8 e;
      e.time_ms = pair[0].toInt();
      e.values = static_cast<uint8_t>(pair[1].toUInt());
      sequence_.append(e);
    }
  }
  std::sort(sequence_.begin(), sequence_.end(),
            [](const SequenceEntry8 &a, const SequenceEntry8 &b) {
              return a.time_ms < b.time_ms;
            });
}

// ------------------------------------------------------------------
// SignalSource8Component (macro generated boilerplate)
// ------------------------------------------------------------------

COMPONENT_CLASS_DEFINITION(SignalSource8, 2, 2)

void SignalSource8Component::onSettingsBtnClicked() {
  auto raw = qobject_cast<SignalSource8RawComponent *>(raw_component_);
  auto dialog = new SignalSource8SettingsDialog(raw, this, this);
  dialog->exec();
  delete dialog;
}

// ------------------------------------------------------------------
// SignalSource8SettingsDialog
// ------------------------------------------------------------------

SignalSource8SettingsDialog::SignalSource8SettingsDialog(
    SignalSource8RawComponent *raw_component, AbstractComponent *component,
    QWidget *parent)
    : ComponentSettingsDialog(component, parent),
      raw_component_(raw_component) {
  initUi();
}

SignalSource8SettingsDialog::~SignalSource8SettingsDialog() {}

void SignalSource8SettingsDialog::initUi() {
  // Sequence table (9 columns: Time + SIG0~SIG7)
  auto seq_group = new QGroupBox(tr("Sequence Editor (cycles, SIG0~SIG7)"), this);
  auto seq_main_layout = new QVBoxLayout(seq_group);

  sequence_table_ = new QTableWidget(0, 9, this);
  QStringList headers;
  headers << tr("Time (cycles)");
  for (int i = 0; i < 8; ++i) headers << QString("SIG%1").arg(i);
  sequence_table_->setHorizontalHeaderLabels(headers);
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

  connect(add_row_btn_, &QPushButton::clicked, this,
          &SignalSource8SettingsDialog::onAddRow);
  connect(del_row_btn_, &QPushButton::clicked, this,
          &SignalSource8SettingsDialog::onDeleteRow);
}

void SignalSource8SettingsDialog::refreshSequenceTable() {
  const auto &seq = raw_component_->sequence();
  sequence_table_->setRowCount(seq.size());
  for (int i = 0; i < seq.size(); ++i) {
    // time_ms now stores cycles directly
    sequence_table_->setItem(
        i, 0, new QTableWidgetItem(QString::number(seq[i].time_ms)));
    for (int j = 0; j < 8; ++j) {
      bool val = (seq[i].values >> j) & 1;
      sequence_table_->setItem(i, j + 1,
                               new QTableWidgetItem(val ? "1" : "0"));
    }
  }
}

void SignalSource8SettingsDialog::onAddRow() {
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
  for (int j = 1; j < 9; ++j) {
    sequence_table_->setItem(row, j, new QTableWidgetItem(QString("0")));
  }
}

void SignalSource8SettingsDialog::onDeleteRow() {
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

void SignalSource8SettingsDialog::accept() {
  // Save sequence (stored directly in cycles, no conversion)
  QVector<SequenceEntry8> new_sequence;
  for (int i = 0; i < sequence_table_->rowCount(); ++i) {
    SequenceEntry8 e;
    e.time_ms = sequence_table_->item(i, 0)->text().toInt(); // cycles
    e.values = 0;
    for (int j = 0; j < 8; ++j) {
      bool val = sequence_table_->item(i, j + 1)->text().toInt() != 0;
      if (val) e.values |= (1 << j);
    }
    new_sequence.append(e);
  }
  std::sort(new_sequence.begin(), new_sequence.end(),
            [](const SequenceEntry8 &a, const SequenceEntry8 &b) {
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
