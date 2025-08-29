#ifndef IOT_MUSIC_PLAYER_H
#define IOT_MUSIC_PLAYER_H

#include "iot/thing.h"

namespace iot {

class MusicPlayerThing : public Thing {
public:
    MusicPlayerThing();
    void Invoke(const cJSON* command) override;
private:
    // 直接处理JSON命令的方法
    void HandleShowCommand(const cJSON* parameters);
    void HandleHideCommand(const cJSON* parameters);

    
    // IoT框架参数处理方法（暂时保留）
    void HandleShowMethod(const ParameterList& parameters);
    void HandleHideMethod(const ParameterList& parameters);

    
    // 保留原有的JSON处理方法（内部使用）
    bool HandleShow(const cJSON* parameters);
    bool HandleHide(const cJSON* parameters);

};

} // namespace iot

#endif // IOT_MUSIC_PLAYER_H
