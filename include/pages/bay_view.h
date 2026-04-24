#pragma once

#include <cstdio>

#include "pages/base_page.h"
#include "pages/resource_list.h"
#include "pages/item_list.h"
#include "state/facility.h"

typedef enum
{
    BT_SHUTTLE,
    BT_SPACE,
    BT_COUNT
} BayType;

extern const char *bayTypeName[BT_COUNT];

class BayView : public BasePage
{
    SublocationType sublocationType;
    BayType type;
    Facility *facility;
    const TextureAsset *partsTexture;

    int section;
    int targetSection;
    float offset;
    int driveSection;

    ResourceList resourceList;
    ItemList itemList;

public:
    BayView(SublocationType s, BayType bt) : sublocationType{s}, type{bt}, section{0}, targetSection{0}, offset{0.0f}, driveSection{2}, resourceList{nullptr, {0, 0, 0, 0}}, itemList{{0, 0, 0, 0}}
    {
        backgroundSource = pageBackgroundSources[PB_HANGAR];
        standardButtons = ALL_STANDARD_BUTTONS;
        partsTexture = TextureManager::getInstance().getTexture(TEXTURE_ITEMS);
    }
    ~BayView() {}

    void activate(ViewState &viewState) override;
    void input() override;
    void render() override;

    Shuttle *getShuttle();

    void renderShuttle(Shuttle *shuttle);
    void renderPod(Pod *pod);
    void renderDrive(Shuttle *shuttle);

    // UI interaction
    void loadToolPod(Pod *pod);
    void loadSupplyPod(Pod *pod);
};