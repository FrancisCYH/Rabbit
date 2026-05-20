#pragma once
#ifndef SIGNAL_SOURCE_8_COMPONENT_H
#define SIGNAL_SOURCE_8_COMPONENT_H

#include <QElapsedTimer>
#include <QTableWidget>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"
#include "Components/ComponentSettingsDialog.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(SignalSource8)

/// @brief A single entry in the 8-port timing sequence.
/// Note: time_ms is stored in cycles (not ms).
struct SequenceEntry8 {
  int time_ms = 0;  // Actually stores cycles, kept for backward compat
  uint8_t values = 0; // bit i = value for SIGi

  bool operator==(const SequenceEntry8 &other) const {
    return time_ms == other.time_ms && values == other.values;
  }
};

/// @brief SignalSource8RawComponent class
/// 8-port programmable signal source with unified time-base (Sequence only).
class SignalSource8RawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  SignalSource8RawComponent(QWidget *parent = nullptr);
  virtual ~SignalSource8RawComponent();

  void setFrequency(int frequency) noexcept override {
    AbstractRawComponent::setFrequency(frequency);
    update();
  }

  void reset() override;
  void processReadData(QQueue<uint64_t> &read_queue) override;
  uint64_t getWriteData() const override;

  // Getters for settings
  const QVector<SequenceEntry8> &sequence() const noexcept { return sequence_; }
  bool sequenceLoop() const noexcept { return sequence_loop_; }
  bool sequenceHold() const noexcept { return sequence_hold_; }

  // Setters for settings
  void setSequence(const QVector<SequenceEntry8> &seq) { sequence_ = seq; }
  void setSequenceLoop(bool loop) noexcept { sequence_loop_ = loop; }
  void setSequenceHold(bool hold) noexcept { sequence_hold_ = hold; }

  // Deprecated: kept for backward compatibility, do not use for new logic.
  // All time values are now stored directly in cycles.
  int timeToCycles(int time_ms) const {
    if (frequency_ <= 0) return time_ms;
    return time_ms * frequency_ / 1000;
  }
  int cyclesToTime(int cycles) const {
    if (frequency_ <= 0) return cycles;
    return cycles * 1000 / frequency_;
  }

  // Serialization helpers for project save/load
  QString sequenceToString() const;
  void sequenceFromString(const QString &str);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

  void initPorts() override;

private:
  uint8_t computeOutput() const;
  void drawSequencePreview(QPainter &painter, const QRect &rect) const;

  QVector<SequenceEntry8> sequence_;
  bool sequence_loop_ = false;
  bool sequence_hold_ = true;

  mutable int cycle_counter_ = 0;
};

/// @brief SignalSource8SettingsDialog class
/// Custom settings dialog for 8-port SignalSource (sequence only).
class SignalSource8SettingsDialog : public ComponentSettingsDialog {
  Q_OBJECT

public:
  SignalSource8SettingsDialog(SignalSource8RawComponent *raw_component,
                              AbstractComponent *component,
                              QWidget *parent = nullptr);
  virtual ~SignalSource8SettingsDialog();

  void accept() override;

private:
  void initUi();
  void onAddRow();
  void onDeleteRow();
  void refreshSequenceTable();

  SignalSource8RawComponent *raw_component_;

  QTableWidget *sequence_table_;
  QCheckBox *loop_check_;
  QCheckBox *hold_check_;
  QPushButton *add_row_btn_;
  QPushButton *del_row_btn_;

  QRadioButton *active_high_radio_;
  QRadioButton *active_low_radio_;
};

} // namespace rabbit_App::component

#endif // SIGNAL_SOURCE_8_COMPONENT_H
