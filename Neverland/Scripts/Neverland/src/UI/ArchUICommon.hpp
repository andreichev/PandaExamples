#pragma once

#include "../BlocksCreation.hpp"

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Общие виджеты и контекст Библиотеки архитектуры (панели разделов — отдельные файлы).

// Карточка выбора (материал/элемент): превью или крупная подпись + имя, подсветка выбора.
class BlockCardButton final : public PandaUI::Button {
public:
    BlockCardButton(const char *name, PandaUI::TextureHandle preview, const char *fallbackText);

    void setCardSelected(bool selected);

private:
    void controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) override;
    void themeChanged() override;
    void updateAppearance();

    bool m_selected;
};

// Крупная кнопка меню (пауза, Close). Живёт здесь — общая для HUD и библиотеки.
class MenuButton final : public PandaUI::Button {
public:
    explicit MenuButton(std::string text);

private:
    void controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) override;
    void themeChanged() override;
    void updateAppearance();
};

// Кнопка пресета в редакторе раздела: компактная, с подсветкой выбранного значения.
class SettingsPresetButton final : public PandaUI::Button {
public:
    explicit SettingsPresetButton(std::string text);

    void setSelected(bool selected);

private:
    void controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) override;
    void themeChanged() override;
    void updateAppearance();

    bool m_selected = false;
};

// Контекст панелей: доступ к выбору игрока, превью тайлов и реестрам карточек
// (реестры — для глобальной подсветки текущего выбора, владелец — ArchLibraryMenu).
struct ArchUIContext {
    Shared<BlocksCreation> blocks;
    std::function<PandaUI::TextureHandle(VoxelType)> previewFor;
    std::vector<std::pair<VoxelType, std::shared_ptr<BlockCardButton>>> *blockCards = nullptr;
    std::vector<std::pair<ArchObjectType, std::shared_ptr<BlockCardButton>>> *elementCards = nullptr;
};

// Базовый класс панели раздела: refresh() зовётся при показе (синк пресетов с состоянием).
class ArchPanel : public PandaUI::Panel {
public:
    ArchPanel();
    virtual void refresh() {}
    // true — панель сама прокручивает контент (CollectionView внутри) и растягивается
    // на высоту раздела; false — панель кладётся в общий ScrollView меню.
    virtual bool hasOwnScrolling() const {
        return false;
    }
};

namespace ArchUI {

// Показ/скрытие с участием в лэйауте: setHidden сам по себе оставляет место в потоке.
void setViewVisible(PandaUI::View &view, bool visible);

std::shared_ptr<PandaUI::Label> sectionLabel(const char *text);
std::shared_ptr<PandaUI::Label> noteLabel(const char *text);
std::shared_ptr<PandaUI::Panel> cardsRow();
std::shared_ptr<PandaUI::Panel> divider();

// Карточка материала: клик выбирает материал и форму-куб.
std::shared_ptr<BlockCardButton> makeMaterialCard(
    const ArchUIContext &context, VoxelType type, const char *name
);
// Карточка формы: клик выбирает элемент.
std::shared_ptr<BlockCardButton> makeElementCard(
    const ArchUIContext &context, ArchObjectType element, const char *name, const char *hint
);

} // namespace ArchUI
