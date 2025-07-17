#include "analysis_pipeline/sbfoster_midas_event_unpacker/stages/sbfoster_midas_event_unpacker_stage.h"

#include <TClass.h>
#include <TROOT.h>
#include <TClassTable.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

ClassImp(SbfosterMidasEventUnpackerStage)

SbfosterMidasEventUnpackerStage::SbfosterMidasEventUnpackerStage() = default;
SbfosterMidasEventUnpackerStage::~SbfosterMidasEventUnpackerStage() = default;

void SbfosterMidasEventUnpackerStage::OnInit() {
    unpackerClassName_ = parameters_.at("unpacker_class").get<std::string>();
    spdlog::debug("[{}] Instantiating unpacker '{}'", Name(), unpackerClassName_);

    TClass* cls = TClass::GetClass(unpackerClassName_.c_str());
    if (!cls) {
        throw std::runtime_error("Unknown unpacker class: " + unpackerClassName_);
    }

    TObject* obj = static_cast<TObject*>(cls->New());
    unpacker_.reset(dynamic_cast<unpackers::EventUnpacker*>(obj));
    if (!unpacker_) {
        delete obj;
        throw std::runtime_error("Unpacker is not derived from EventUnpacker");
    }

    spdlog::debug("[{}] Successfully created unpacker '{}'", Name(), unpackerClassName_);
}

void SbfosterMidasEventUnpackerStage::ProcessMidasEvent(std::shared_ptr<TMEvent> event) {
    if (!unpacker_) return;

    event->FindAllBanks();
    unpacker_->UnpackEvent(event.get());

    const auto& collections = unpacker_->GetCollections();
    spdlog::debug("[{}] Unpacked {} collections", Name(), collections.size());

    std::vector<std::pair<std::string, std::unique_ptr<PipelineDataProduct>>> productsVec;
    productsVec.reserve(collections.size());

    for (const auto& [label, vecPtr] : collections) {
        if (!vecPtr || vecPtr->empty()) continue;

        auto list = std::make_unique<TList>();
        list->SetOwner(kTRUE);

        int added = 0;
        for (const auto& dp : *vecPtr) {
            if (!dp) continue;
            TObject* clone = dp->Clone();
            if (!clone) continue;
            list->Add(clone);
            ++added;
        }

        if (list->IsEmpty()) continue;

        auto product = std::make_unique<PipelineDataProduct>();
        product->setName(label);
        product->setObject(std::move(list));
        product->addTag("unpacked_data");
        product->addTag("built_by_sbfoster_midas_unpacker");

        productsVec.emplace_back(label, std::move(product));

        spdlog::debug("[{}] Produced '{}' with {} objects", Name(), label, added);
    }

    if (!productsVec.empty()) {
        getDataProductManager()->addOrUpdateMultiple(std::move(productsVec));
    }
}

std::string SbfosterMidasEventUnpackerStage::Name() const {
    return "SbfosterMidasEventUnpackerStage";
}
