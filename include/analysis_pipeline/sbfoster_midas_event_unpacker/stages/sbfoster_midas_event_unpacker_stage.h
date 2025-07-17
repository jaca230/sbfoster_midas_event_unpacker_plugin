#ifndef STAGES_UNPACKING_SBFOSTER_MIDAS_EVENT_UNPACKER_STAGE_H
#define STAGES_UNPACKING_SBFOSTER_MIDAS_EVENT_UNPACKER_STAGE_H

#include <memory>
#include <string>
#include <unordered_map>

#include <TObject.h>
#include <TList.h>
#include <nlohmann/json.hpp>

#include "analysis_pipeline/midas_event_unpacker/stages/midas_event_unpacker_stage.h"
#include "unpacker/common/unpacking/EventUnpacker.hh"

/**
 * A generic MIDAS unpacker stage that dynamically instantiates an EventUnpacker
 * based on configuration. Compatible with TMEvent input and emits TList-backed
 * PipelineDataProducts from unpacked collections.
 */
class SbfosterMidasEventUnpackerStage : public MidasEventUnpackerStage {
public:
    SbfosterMidasEventUnpackerStage();
    ~SbfosterMidasEventUnpackerStage() override;

    std::string Name() const override;

protected:
    void OnInit() override;
    void ProcessMidasEvent(std::shared_ptr<TMEvent> event) override;

private:
    std::unique_ptr<unpackers::EventUnpacker> unpacker_;
    std::string unpackerClassName_;

    ClassDefOverride(SbfosterMidasEventUnpackerStage, 1);
};

#endif // STAGES_UNPACKING_SBFOSTER_MIDAS_EVENT_UNPACKER_STAGE_H
