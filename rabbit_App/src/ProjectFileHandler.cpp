#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "Components/Components.h"
#include "Components/ComponentsFactory.h"
#include "Components/SignalSourceComponent.h"
#include "Components/SignalSource8Component.h"
#include "Ports/PinInfo.h"
#include "Ports/Port.h"
#include "ProjectFileHandler.h"

#include "Components/AbstractComponent.h"

using namespace rabbit_App;
using namespace rabbit_App::component;

ProjectFileHandler::ProjectFileHandler() { waveform_enabled_ = true; }

ProjectFileHandler::~ProjectFileHandler() {}

void ProjectFileHandler::readProjectFromFile(const QString &project_path) {
  QXmlStreamReader xml_reader;
  QFile file(project_path);
  file.open(QIODevice::ReadOnly);
  if (!file.isOpen()) {
    throw std::runtime_error("Failed to open file.");
  }
  xml_reader.setDevice(&file);
  component::AbstractComponent *current_component = nullptr;
  while (!xml_reader.atEnd()) {
    xml_reader.readNext();
    auto name = xml_reader.name().toString();
    if (xml_reader.isStartElement()) {
      if (name == "project") {
        project_name_ = xml_reader.attributes().value("name").toString();
        constraint_path_ =
            xml_reader.attributes().value("constraint").toString();
        components_panel_->setConstraintFilePath(constraint_path_);
        bitstream_path_ = xml_reader.attributes().value("bitstream").toString();
        auto waveform_enabled_attr =
            xml_reader.attributes().value("waveform_enabled");
        waveform_enabled_ = waveform_enabled_attr.isEmpty()
                                 ? true
                                 : waveform_enabled_attr.toString() == "true";
      } else if (name == "panel") {
        auto grid_width = xml_reader.attributes().value("grid_width").toInt();
        auto grid_height = xml_reader.attributes().value("grid_height").toInt();
        components_panel_->setGridSize(grid_width, grid_height);
      } else if (name == "components") {

      } else if (name == "component") {
        auto typeStr = xml_reader.attributes().value("type").toString();
        auto nameStr = xml_reader.attributes().value("name").toString();
        auto grid_x = xml_reader.attributes().value("grid_x").toInt();
        auto grid_y = xml_reader.attributes().value("grid_y").toInt();
        current_component = component::ComponentsFactory::create(typeStr);
        current_component->setComponentName(nameStr);
        components_panel_->appendComponent(current_component, grid_y, grid_x);
      } else if (name == "settings") {

      } else if (name == "setting") {
        readComponentsSettings(xml_reader, current_component);
      } else if (name == "inputs") {

      } else if (name == "input") {
        auto pin = xml_reader.attributes().value("pin").toString();
        auto port = xml_reader.attributes().value("port").toString();
        // auto &component_inputs = current_component->inputPorts();
        auto &input = current_component->rawComponent()->findPort(
            port, ports::PortType::Input);
        if (input.name == port) {
          input.pin_name = pin;
          input.pin_index = ports::declIndexMap(pin).second;
        }
      } else if (name == "outputs") {

      } else if (name == "output") {
        auto pin = xml_reader.attributes().value("pin").toString();
        auto port = xml_reader.attributes().value("port").toString();
        // auto &component_outputs = current_component->outputPorts();
        auto &output = current_component->rawComponent()->findPort(
            port, ports::PortType::Output);
        if (output.name == port) {
          output.pin_name = pin;
          output.pin_index = ports::declIndexMap(pin).second;
        }
      }
    } else if (xml_reader.isEndElement()) {
      if (name == "component") {
        current_component = nullptr;
      }
    }
  }

  if (xml_reader.hasError()) {
    throw std::runtime_error(xml_reader.errorString().toLocal8Bit());
  }
  file.close();
}

void ProjectFileHandler::writeProjectToFile(const QString &project_path) {

  QXmlStreamWriter xml_writer;
  QFile file(project_path);
  file.open(QIODevice::WriteOnly);
  if (!file.isOpen()) {
    throw std::runtime_error("Failed to open file.");
  }
  xml_writer.setDevice(&file);
  xml_writer.setAutoFormatting(true);
  xml_writer.writeStartDocument();
  xml_writer.writeComment("Rabbit Project File");

  // Project Information
  xml_writer.writeStartElement("project");
  xml_writer.writeAttribute("name", project_name_);
  xml_writer.writeAttribute("constraint", constraint_path_);
  xml_writer.writeAttribute("bitstream", bitstream_path_);
  xml_writer.writeAttribute("waveform_enabled",
                            waveform_enabled_ ? "true" : "false");
  // xml_writer.writeEndElement();

  // Components Information
  xml_writer.writeStartElement("panel");
  xml_writer.writeAttribute("grid_width",
                            QString::number(components_panel_->gridWidth()));
  xml_writer.writeAttribute("grid_height",
                            QString::number(components_panel_->gridHeight()));
  xml_writer.writeEndElement();

  xml_writer.writeStartElement("components");
  for (const auto component : components_panel_->components()) {
    auto component_grid_pos = components_panel_->componentGridCoords(component);
    xml_writer.writeStartElement("component");
    xml_writer.writeAttribute("type", component->componentType());
    xml_writer.writeAttribute("name", component->componentName());
    xml_writer.writeAttribute("grid_x",
                              QString::number(component_grid_pos.x()));
    xml_writer.writeAttribute("grid_y",
                              QString::number(component_grid_pos.y()));

    // Settings
    xml_writer.writeStartElement("settings");
    writeComponentsSettings(xml_writer, component);
    xml_writer.writeEndElement();

    // Inputs
    xml_writer.writeStartElement("inputs");
    auto input_vec = component->inputPorts();
    for (const auto &port : input_vec) {
      xml_writer.writeStartElement("input");
      xml_writer.writeAttribute("pin", port.pin_name);
      xml_writer.writeAttribute("port", port.name);
      xml_writer.writeEndElement();
    }
    xml_writer.writeEndElement();

    // Outputs
    xml_writer.writeStartElement("outputs");
    auto output_vec = component->outputPorts();
    for (const auto &port : output_vec) {
      xml_writer.writeStartElement("output");
      xml_writer.writeAttribute("pin", port.pin_name);
      xml_writer.writeAttribute("port", port.name);
      xml_writer.writeEndElement();
    }
    xml_writer.writeEndElement();

    xml_writer.writeEndElement();
  }
  xml_writer.writeEndElement();

  xml_writer.writeEndElement();
  xml_writer.writeEndDocument();
  file.close();
}

void ProjectFileHandler::readComponentsSettings(
    QXmlStreamReader &xml_reader,
    rabbit_App::component::AbstractComponent *current_component) {
  auto raw_component = current_component->rawComponent();
  auto setting = xml_reader.attributes().value("name").toString();
  auto value = xml_reader.attributes().value("value").toString();
  if (setting == "is_low_active") {
    raw_component->setLowActive(value == "true");
  } else if (setting == "color") {
    // TODO :the string of usage here should support multi languige
    // identification
    auto usage = xml_reader.attributes().value("usage").toString();
    auto color = raw_component->supportedColors()[value];
    raw_component->setComponentColor(usage, color);
  } else if (setting == "vision_persistence") {
    auto vision_persistence = value.toInt();
    raw_component->setVisionPersistence(vision_persistence);
  } else if (setting == "signal_mode") {
    auto mode = static_cast<rabbit_App::component::SignalMode>(value.toInt());
    auto *sig = qobject_cast<rabbit_App::component::SignalSourceRawComponent *>(
        raw_component);
    if (sig) sig->setSignalMode(mode);
  } else if (setting == "signal_period") {
    auto *sig = qobject_cast<rabbit_App::component::SignalSourceRawComponent *>(
        raw_component);
    if (sig) sig->setPeriodCycles(value.toInt());
  } else if (setting == "signal_duty") {
    auto *sig = qobject_cast<rabbit_App::component::SignalSourceRawComponent *>(
        raw_component);
    if (sig) sig->setDutyCyclePercent(value.toInt());
  } else if (setting == "signal_delay") {
    auto *sig = qobject_cast<rabbit_App::component::SignalSourceRawComponent *>(
        raw_component);
    if (sig) sig->setDelayCycles(value.toInt());
  } else if (setting == "signal_pulse_width") {
    auto *sig = qobject_cast<rabbit_App::component::SignalSourceRawComponent *>(
        raw_component);
    if (sig) sig->setPulseWidthCycles(value.toInt());
  } else if (setting == "signal_sequence") {
    auto *sig = qobject_cast<rabbit_App::component::SignalSourceRawComponent *>(
        raw_component);
    if (sig) {
      sig->sequenceFromString(value);
      auto loop_attr = xml_reader.attributes().value("loop").toString();
      auto hold_attr = xml_reader.attributes().value("hold").toString();
      sig->setSequenceLoop(loop_attr == "true");
      sig->setSequenceHold(hold_attr == "true");
    }
  } else if (setting == "signal8_sequence") {
    auto *sig = qobject_cast<rabbit_App::component::SignalSource8RawComponent *>(
        raw_component);
    if (sig) {
      sig->sequenceFromString(value);
      auto loop_attr = xml_reader.attributes().value("loop").toString();
      auto hold_attr = xml_reader.attributes().value("hold").toString();
      sig->setSequenceLoop(loop_attr == "true");
      sig->setSequenceHold(hold_attr == "true");
    }
  }
}

void ProjectFileHandler::writeComponentsSettings(
    QXmlStreamWriter &xml_writer,
    rabbit_App::component::AbstractComponent *current_component) {
  // active type
  auto type = current_component->componentType();
  auto raw_component = current_component->rawComponent();
  xml_writer.writeStartElement("setting");
  xml_writer.writeAttribute("name", "is_low_active");
  xml_writer.writeAttribute("value",
                            raw_component->isLowActive() ? "true" : "false");
  xml_writer.writeEndElement();

  // color
  auto colors_map = raw_component->componentColors();
  auto supported_colors_map = raw_component->supportedColors();
  for (auto itor = colors_map.begin(); itor != colors_map.end(); ++itor) {
    xml_writer.writeStartElement("setting");
    xml_writer.writeAttribute("name", "color");
    xml_writer.writeAttribute("usage", itor.key());
    xml_writer.writeAttribute("value", supported_colors_map.key(itor.value()));
    xml_writer.writeEndElement();
  }

  // vision persistence
  if (type == "LED4x4Matrix" || type == "LED8x8Matrix" ||
      type == "LED16x16Matrix" || type == "FourDigitSegmentDisplay") {
    auto vision_persistence = raw_component->visionPersistence();
    xml_writer.writeStartElement("setting");
    xml_writer.writeAttribute("name", "vision_persistence");
    xml_writer.writeAttribute("value", QString::number(vision_persistence));
    xml_writer.writeEndElement();
  }

  // SignalSource settings
  if (type == "SignalSource") {
    auto *sig = qobject_cast<rabbit_App::component::SignalSourceRawComponent *>(
        raw_component);
    if (sig) {
      xml_writer.writeStartElement("setting");
      xml_writer.writeAttribute("name", "signal_mode");
      xml_writer.writeAttribute("value",
                                QString::number(static_cast<int>(sig->signalMode())));
      xml_writer.writeEndElement();

      xml_writer.writeStartElement("setting");
      xml_writer.writeAttribute("name", "signal_period");
      xml_writer.writeAttribute("value", QString::number(sig->periodCycles()));
      xml_writer.writeEndElement();

      xml_writer.writeStartElement("setting");
      xml_writer.writeAttribute("name", "signal_duty");
      xml_writer.writeAttribute("value",
                                QString::number(sig->dutyCyclePercent()));
      xml_writer.writeEndElement();

      xml_writer.writeStartElement("setting");
      xml_writer.writeAttribute("name", "signal_delay");
      xml_writer.writeAttribute("value", QString::number(sig->delayCycles()));
      xml_writer.writeEndElement();

      xml_writer.writeStartElement("setting");
      xml_writer.writeAttribute("name", "signal_pulse_width");
      xml_writer.writeAttribute("value", QString::number(sig->pulseWidthCycles()));
      xml_writer.writeEndElement();

      xml_writer.writeStartElement("setting");
      xml_writer.writeAttribute("name", "signal_sequence");
      xml_writer.writeAttribute("value", sig->sequenceToString());
      xml_writer.writeAttribute("loop", sig->sequenceLoop() ? "true" : "false");
      xml_writer.writeAttribute("hold", sig->sequenceHold() ? "true" : "false");
      xml_writer.writeEndElement();
    }
  }

  // SignalSource8 settings
  if (type == "SignalSource8") {
    auto *sig = qobject_cast<rabbit_App::component::SignalSource8RawComponent *>(
        raw_component);
    if (sig) {
      xml_writer.writeStartElement("setting");
      xml_writer.writeAttribute("name", "signal8_sequence");
      xml_writer.writeAttribute("value", sig->sequenceToString());
      xml_writer.writeAttribute("loop", sig->sequenceLoop() ? "true" : "false");
      xml_writer.writeAttribute("hold", sig->sequenceHold() ? "true" : "false");
      xml_writer.writeEndElement();
    }
  }
}
