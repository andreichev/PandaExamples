#pragma once

#include "ArchUICommon.hpp"

// Библиотека архитектуры (меню M): полноэкранный каталог — слева разделы, справа
// специализированный редактор раздела (каждая панель — свой файл UI/*Panel.cpp).
class ArchLibraryMenu final : public PandaUI::Panel {
public:
    ArchLibraryMenu(
        Shared<BlocksCreation> blocks, std::function<PandaUI::TextureHandle(VoxelType)> previewFor
    );

    // Подсветка карточек всех панелей + синк редактора активного раздела.
    void syncSelection(VoxelType block, ArchObjectType element);

private:
    void setSection(size_t index);

    struct Section {
        std::shared_ptr<ArchPanel> panel;
        std::shared_ptr<PandaUI::Button> button;
    };

    // Реестры карточек (заполняют панели при создании через ArchUIContext).
    std::vector<std::pair<VoxelType, std::shared_ptr<BlockCardButton>>> m_blockCards;
    std::vector<std::pair<ArchObjectType, std::shared_ptr<BlockCardButton>>> m_elementCards;
    std::vector<Section> m_sections;
    std::shared_ptr<PandaUI::Label> m_sectionTitle;
    // Общая прокрутка разделов; панели с hasOwnScrolling живут вне её (без вложенного скролла).
    std::shared_ptr<PandaUI::ScrollView> m_scroll;
    size_t m_activeSection = 0;
};
