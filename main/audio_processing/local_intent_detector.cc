#include "local_intent_detector.h"
#include "board.h"
#include "display.h"
#include <esp_log.h>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <cstdlib>
#include <set>
#include <vector>
#include <cmath>
#include <climits>
#include <limits>

static const char* TAG = "LocalIntentDetector";

namespace intent {

LocalIntentDetector::LocalIntentDetector() 
    : enabled_(true) {
}

LocalIntentDetector::~LocalIntentDetector() {
}

void LocalIntentDetector::Initialize() {
    ESP_LOGI(TAG, "初始化本地意图检测器");
    InitializeDefaultRules();
}

void LocalIntentDetector::InitializeDefaultRules() {
    // 屏幕亮度控制规则 - 放在前面，优先匹配更具体的关键词
    KeywordRule brightness_rule;
    
#ifdef CONFIG_LANGUAGE_ZH_CN
    // 简体中文
    brightness_rule.keywords = {
        "亮度", "屏幕亮度", "调亮", "调暗", "屏幕", "亮点", "暗点",
        "调节亮度", "设置亮度", "屏幕调", "亮度调", "变亮", "变暗", "屏幕亮度调",
        "亮度最大", "亮度最小", "亮度大一点", "亮度小一点", "屏幕亮一点", "屏幕暗一点",
        "亮度调到最大", "亮度调到最小", "屏幕调到最大", "屏幕调到最小",
        "最亮", "最暗", "调到最亮", "调到最暗"
    };
#elif defined(CONFIG_LANGUAGE_EN_US)
    // 英文
    brightness_rule.keywords = {
        "brightness", "screen brightness", "brighten", "dim", "screen",
        "adjust brightness", "set brightness", "brightness up", "brightness down",
        "maximum brightness", "minimum brightness", "brighter", "darker",
        "brightest", "dimmest", "a bit brighter", "a bit darker",
        "increase brightness", "decrease brightness", "make brighter", "make darker",
        "brightness to max", "brightness to min", "screen to max", "screen to min",
        "turn up brightness", "turn down brightness", "raise brightness", "lower brightness",
        "brightness little up", "brightness little down"
    };
#elif defined(CONFIG_LANGUAGE_JA_JP)
    // 日文
    brightness_rule.keywords = {
        "明るさ", "画面の明るさ", "明るく", "暗く", "画面",
        "明るさ調整", "明るさ設定", "明るさ上げ", "明るさ下げ",
        "最大の明るさ", "最小の明るさ", "もっと明るく", "もっと暗く",
        "最大限", "最小限", "少し明るく", "少し暗く",
        "明るくする", "暗くする", "明るさを上げる", "明るさを下げる",
        "明るさ最大", "明るさ最小", "画面明るく", "画面暗く",
        "明るさ調節", "画面調整", "少しだけ明るく", "少しだけ暗く"
    };
#elif defined(CONFIG_LANGUAGE_KO_KR)
    // 韩文
    brightness_rule.keywords = {
        "밝기", "화면 밝기", "밝게", "어둡게", "화면",
        "밝기 조절", "밝기 설정", "밝기 올리기", "밝기 내리기",
        "최대 밝기", "최소 밝기", "더 밝게", "더 어둡게",
        "가장 밝게", "가장 어둡게", "조금 밝게", "조금 어둡게",
        "밝게 하기", "어둡게 하기", "밝기를 높이다", "밝기를 낮추다",
        "밝기 최대", "밝기 최소", "화면 밝게", "화면 어둡게",
        "밝기 높이기", "밝기 낮추기", "조금만 밝게", "조금만 어둡게"
    };
#elif defined(CONFIG_LANGUAGE_TH_TH)
    // 泰文
    brightness_rule.keywords = {
        "ความสว่าง", "ความสว่างหน้าจอ", "สว่างขึ้น", "มืดลง", "หน้าจอ",
        "ปรับความสว่าง", "ตั้งความสว่าง", "เพิ่มความสว่าง", "ลดความสว่าง",
        "ความสว่างสูงสุด", "ความสว่างต่ำสุด", "สว่างขึ้นอีก", "มืดลงอีก",
        "สว่างที่สุด", "มืดที่สุด", "สว่างขึ้นนิดหน่อย", "มืดลงนิดหน่อย",
        "ทำให้สว่าง", "ทำให้มืด",
        "ปรับหน้าจอ", "ความสว่างสูง", "ความสว่างต่ำ", "สว่างมาก", "มืดมาก",
        "เพิ่มขึ้น", "ลดลง", "สว่างเล็กน้อย", "มืดเล็กน้อย"
    };
#elif defined(CONFIG_LANGUAGE_VI_VN)
    // 越南文
    brightness_rule.keywords = {
        "độ sáng", "độ sáng màn hình", "sáng hơn", "tối hơn", "màn hình",
        "điều chỉnh độ sáng", "đặt độ sáng", "tăng độ sáng", "giảm độ sáng",
        "độ sáng tối đa", "độ sáng tối thiểu", "sáng hơn nữa", "tối hơn nữa",
        "sáng nhất", "tối nhất", "sáng lên một chút", "tối đi một chút",
        "làm sáng", "làm tối",
        "độ sáng cao", "độ sáng thấp", "màn hình sáng", "màn hình tối",
        "tăng lên", "giảm xuống", "sáng thêm", "tối bớt"
    };
#else
    // 默认简体中文
    brightness_rule.keywords = {
        "亮度", "屏幕亮度", "调亮", "调暗", "屏幕", "亮点", "暗点",
        "调节亮度", "设置亮度", "屏幕调", "亮度调", "变亮", "变暗", "屏幕亮度调",
        "亮度最大", "亮度最小", "亮度大一点", "亮度小一点", "屏幕亮一点", "屏幕暗一点",
        "亮度调到最大", "亮度调到最小", "屏幕调到最大", "屏幕调到最小",
        "最亮", "最暗", "调到最亮", "调到最暗"
    };
#endif
    
    brightness_rule.intent_type = IntentType::BRIGHTNESS_CONTROL;
    brightness_rule.action = "SetBrightness";
    brightness_rule.device = "Screen";
    brightness_rule.parameter_extractor = ExtractBrightnessParameters;
    AddKeywordRule(brightness_rule);
    
    // 音量控制规则
    KeywordRule volume_rule;
    
#ifdef CONFIG_LANGUAGE_ZH_CN
    // 简体中文
    volume_rule.keywords = {
        "音量", "声音", "大声", "小声", "调节音量", "设置音量", "音量调",
        "音量调到", "音量设为", "音量调成", "音量变成",
        "音量最大", "音量最小", "音量大一点", "音量小一点", "声音大一点", "声音小一点",
        "音量调到最大", "音量调到最小", "声音调到最大", "声音调到最小",
        "最响", "静音", "调到最响", "调到静音"
    };
#elif defined(CONFIG_LANGUAGE_EN_US)
    // 英文
    volume_rule.keywords = {
        "volume", "sound", "loud", "quiet", "adjust volume", "set volume",
        "volume up", "volume down", "louder", "quieter",
        "maximum volume", "minimum volume", "a bit louder", "a bit quieter",
        "loudest", "mute", "silence", "increase volume", "decrease volume",
        "turn up", "turn down", "raise volume", "lower volume",
        "volume to max", "volume to min", "sound up", "sound down",
        "volume little up", "volume little down", "make louder", "make quieter",
        "volume higher", "volume lower"
    };
#elif defined(CONFIG_LANGUAGE_JA_JP)
    // 日文
    volume_rule.keywords = {
        "音量", "音", "大きく", "小さく", "音量調整", "音量設定",
        "音量上げ", "音量下げ", "もっと大きく", "もっと小さく",
        "最大音量", "最小音量", "少し大きく", "少し小さく",
        "最大", "ミュート", "消音", "音量を上げる", "音量を下げる",
        "大きくする", "小さくする",
        "音量最大", "音量最小", "音大きく", "音小さく",
        "音量調節", "少しだけ大きく", "少しだけ小さく"
    };
#elif defined(CONFIG_LANGUAGE_KO_KR)
    // 韩文
    volume_rule.keywords = {
        "볼륨", "소리", "크게", "작게", "볼륨 조절", "볼륨 설정",
        "볼륨 올리기", "볼륨 내리기", "더 크게", "더 작게",
        "최대 볼륨", "최소 볼륨", "조금 크게", "조금 작게",
        "가장 크게", "음소거", "무음", "볼륨을 높이다", "볼륨을 낮추다",
        "크게 하기", "작게 하기",
        "볼륨 최대", "볼륨 최소", "소리 크게", "소리 작게",
        "볼륨 높이기", "볼륨 낮추기", "조금만 크게", "조금만 작게"
    };
#elif defined(CONFIG_LANGUAGE_TH_TH)
    // 泰文
    volume_rule.keywords = {
        "ระดับเสียง", "เสียง", "ดังขึ้น", "เบาลง", "ปรับระดับเสียง", "ตั้งระดับเสียง",
        "เพิ่มเสียง", "ลดเสียง", "ดังกว่า", "เบากว่า",
        "เสียงสูงสุด", "เสียงต่ำสุด", "ดังขึ้นนิดหน่อย", "เบาลงนิดหน่อย",
        "ดังที่สุด", "ปิดเสียง", "เงียบ", "เพิ่มระดับเสียง", "ลดระดับเสียง",
        "เสียงดัง", "เสียงเบา", "ดังมาก", "เบามาก",
        "เพิ่มขึ้น", "ลดลง", "ดังเล็กน้อย", "เบาเล็กน้อย"
    };
#elif defined(CONFIG_LANGUAGE_VI_VN)
    // 越南文
    volume_rule.keywords = {
        "âm lượng", "âm thanh", "to hơn", "nhỏ hơn", "điều chỉnh âm lượng", "đặt âm lượng",
        "tăng âm lượng", "giảm âm lượng", "to hơn nữa", "nhỏ hơn nữa",
        "âm lượng tối đa", "âm lượng tối thiểu", "to lên một chút", "nhỏ đi một chút",
        "to nhất", "tắt tiếng", "im lặng", "âm lượng lên", "âm lượng xuống",
        "âm lượng cao", "âm lượng thấp", "tiếng to", "tiếng nhỏ",
        "tăng lên", "giảm xuống", "to thêm", "nhỏ bớt"
    };
#else
    // 默认简体中文
    volume_rule.keywords = {
        "音量", "声音", "大声", "小声", "调节音量", "设置音量", "音量调",
        "音量调到", "音量设为", "音量调成", "音量变成",
        "音量最大", "音量最小", "音量大一点", "音量小一点", "声音大一点", "声音小一点",
        "音量调到最大", "音量调到最小", "声音调到最大", "声音调到最小",
        "最响", "静音", "调到最响", "调到静音"
    };
#endif
    
    volume_rule.intent_type = IntentType::VOLUME_CONTROL;
    volume_rule.action = "SetVolume";
    volume_rule.device = "Speaker";
    volume_rule.parameter_extractor = ExtractVolumeParameters;
    AddKeywordRule(volume_rule);
    
    // 主题控制规则
    KeywordRule theme_rule;
    
#ifdef CONFIG_LANGUAGE_ZH_CN
    // 简体中文
    theme_rule.keywords = {
        "白色主题", "黑色主题", "白天模式", "黑夜模式",
        "白色字体", "黑色字体", "白色字幕", "黑色字幕",
        "亮色主题", "暗色主题"
    };
#elif defined(CONFIG_LANGUAGE_EN_US)
    // 英文
    theme_rule.keywords = {
        "white theme", "black theme", "day mode", "night mode",
        "light theme", "dark theme", "white font", "black font",
        "white subtitle", "black subtitle", "light mode", "dark mode",
        "bright theme", "dim theme", "white text", "black text",
        "light color", "dark color"
    };
#elif defined(CONFIG_LANGUAGE_JA_JP)
    // 日文
    theme_rule.keywords = {
        "白いテーマ", "黒いテーマ", "昼間モード", "夜間モード",
        "ライトテーマ", "ダークテーマ", "白い字", "黒い字",
        "白い字幕", "黒い字幕", "明るいモード", "暗いモード",
        "明るいテーマ", "暗いテーマ", "白テーマ", "黒テーマ"
    };
#elif defined(CONFIG_LANGUAGE_KO_KR)
    // 韩文
    theme_rule.keywords = {
        "흰색 테마", "검은색 테마", "낮 모드", "밤 모드",
        "라이트 테마", "다크 테마", "흰색 글꼴", "검은색 글꼴",
        "흰색 자막", "검은색 자막", "밝은 모드", "어두운 모드",
        "밝은 테마", "어두운 테마", "화이트 테마", "블랙 테마"
    };
#elif defined(CONFIG_LANGUAGE_TH_TH)
    // 泰文
    theme_rule.keywords = {
        "ธีมสีขาว", "ธีมสีดำ", "โหมดกลางวัน", "โหมดกลางคืน",
        "ธีมสว่าง", "ธีมมืด", "ตัวอักษรสีขาว", "ตัวอักษรสีดำ",
        "คำบรรยายสีขาว", "คำบรรยายสีดำ",
        "ธีมสว่างสุด", "ธีมมืดสุด", "สีขาว", "สีดำ"
    };
#elif defined(CONFIG_LANGUAGE_VI_VN)
    // 越南文
    theme_rule.keywords = {
        "chủ đề trắng", "chủ đề đen", "chế độ ngày", "chế độ đêm",
        "chủ đề sáng", "chủ đề tối", "phông chữ trắng", "phông chữ đen",
        "phụ đề trắng", "phụ đề đen", "chế độ sáng", "chế độ tối",
        "giao diện sáng", "giao diện tối", "màu trắng", "màu đen"
    };
#else
    // 默认简体中文
    theme_rule.keywords = {
        "白色主题", "黑色主题", "白天模式", "黑夜模式",
        "白色字体", "黑色字体", "白色字幕", "黑色字幕",
        "亮色主题", "暗色主题"
    };
#endif
    
    theme_rule.intent_type = IntentType::THEME_CONTROL;
    theme_rule.action = "SetTheme";
    theme_rule.device = "Screen";
    theme_rule.parameter_extractor = ExtractThemeParameters;
    AddKeywordRule(theme_rule);
    
    // 显示模式控制规则
    KeywordRule display_mode_rule;
    
#ifdef CONFIG_LANGUAGE_ZH_CN
    // 简体中文
    display_mode_rule.keywords = {
        "静态模式", "动态模式", "静态壁纸", "动态壁纸",
        "静态皮肤", "动态皮肤", "表情包模式", "表情模式",
        "情绪模式", "切换到表情包", "表情包", "emoji模式"
    };
#elif defined(CONFIG_LANGUAGE_EN_US)
    // 英文
    display_mode_rule.keywords = {
        "static mode", "animated mode", "static wallpaper", "animated wallpaper",
        "static skin", "animated skin", "emoticon mode", "emoji mode",
        "emotion mode", "switch to emoticon", "emoticon", "expression mode",
        "still mode", "moving mode", "fixed mode", "dynamic mode",
        "animation mode", "static display", "animated display"
    };
#elif defined(CONFIG_LANGUAGE_JA_JP)
    // 日文
    display_mode_rule.keywords = {
        "静的モード", "動的モード", "静的壁紙", "動的壁紙",
        "静的スキン", "動的スキン", "絵文字モード", "エモーティコンモード",
        "感情モード", "絵文字に切り替え", "絵文字", "表情モード",
        "静止モード", "アニメーションモード", "動くモード", "固定モード"
    };
#elif defined(CONFIG_LANGUAGE_KO_KR)
    // 韩文
    display_mode_rule.keywords = {
        "정적 모드", "동적 모드", "정적 배경화면", "동적 배경화면",
        "정적 스킨", "동적 스킨", "이모티콘 모드", "이모지 모드",
        "감정 모드", "이모티콘으로 전환", "이모티콘", "표정 모드",
        "고정 모드", "움직이는 모드", "애니메이션 모드", "정지 모드"
    };
#elif defined(CONFIG_LANGUAGE_TH_TH)
    // 泰文
    display_mode_rule.keywords = {
        "โหมดคงที่", "โหมดเคลื่อนไหว", "วอลเปเปอร์คงที่", "วอลเปเปอร์เคลื่อนไหว",
        "สกินคงที่", "สกินเคลื่อนไหว", "โหมดอิโมติคอน", "โหมดอีโมจิ",
        "โหมดอารมณ์", "เปลี่ยนเป็นอิโมติคอน", "อิโมติคอน", "โหมดสีหน้า",
        "โหมดนิ่ง", "โหมดเคลื่อนที่", "โหมดแอนิเมชั่น", "แสดงแบบคงที่", "แสดงแบบเคลื่อนไหว"
    };
#elif defined(CONFIG_LANGUAGE_VI_VN)
    // 越南文
    display_mode_rule.keywords = {
        "chế độ tĩnh", "chế độ động", "hình nền tĩnh", "hình nền động",
        "giao diện tĩnh", "giao diện động", "chế độ biểu tượng cảm xúc", "chế độ emoji",
        "chế độ cảm xúc", "chuyển sang biểu tượng cảm xúc", "biểu tượng cảm xúc", "chế độ biểu cảm",
        "chế độ cố định", "chế độ chuyển động", "chế độ hoạt hình", "hiển thị tĩnh", "hiển thị động"
    };
#else
    // 默认简体中文
    display_mode_rule.keywords = {
        "静态模式", "动态模式", "静态壁纸", "动态壁纸",
        "静态皮肤", "动态皮肤", "表情包模式", "表情模式",
        "情绪模式", "切换到表情包", "表情包", "emoji模式"
    };
#endif
    
    display_mode_rule.intent_type = IntentType::DISPLAY_MODE_CONTROL;
    display_mode_rule.action = "SetAnimatedMode"; // 默认动作，会在参数提取器中调整
    display_mode_rule.device = "ImageDisplay";
    display_mode_rule.parameter_extractor = ExtractDisplayModeParameters;
    AddKeywordRule(display_mode_rule);
    
    // 字幕控制规则
    KeywordRule subtitle_rule;
    
#ifdef CONFIG_LANGUAGE_ZH_CN
    // 简体中文
    subtitle_rule.keywords = {
        "打开字幕", "开启字幕", "显示字幕",
        "关闭字幕", "隐藏字幕", "关掉字幕"
    };
#elif defined(CONFIG_LANGUAGE_EN_US)
    // 英文
    subtitle_rule.keywords = {
        "show subtitle", "enable subtitle", "display subtitle",
        "hide subtitle", "disable subtitle", "turn off subtitle",
        "subtitle on", "subtitle off",
        "open subtitle", "close subtitle", "turn on subtitle",
        "subtitle show", "subtitle hide", "activate subtitle", "deactivate subtitle"
    };
#elif defined(CONFIG_LANGUAGE_JA_JP)
    // 日文
    subtitle_rule.keywords = {
        "字幕を表示", "字幕をオン", "字幕を有効",
        "字幕を非表示", "字幕をオフ", "字幕を無効",
        "字幕オン", "字幕オフ",
        "字幕表示", "字幕非表示", "字幕開く", "字幕閉じる",
        "字幕有効化", "字幕無効化"
    };
#elif defined(CONFIG_LANGUAGE_KO_KR)
    // 韩文
    subtitle_rule.keywords = {
        "자막 표시", "자막 켜기", "자막 활성화",
        "자막 숨기기", "자막 끄기", "자막 비활성화",
        "자막 온", "자막 오프",
        "자막 보기", "자막 감추기", "자막 열기", "자막 닫기",
        "자막 보이기", "자막 안보이기"
    };
#elif defined(CONFIG_LANGUAGE_TH_TH)
    // 泰文
    subtitle_rule.keywords = {
        "แสดงคำบรรยาย", "เปิดคำบรรยาย", "เปิดใช้งานคำบรรยาย",
        "ซ่อนคำบรรยาย", "ปิดคำบรรยาย", "ปิดใช้งานคำบรรยาย",
        "คำบรรยายเปิด", "คำบรรยายปิด", "เปิดคำบรรยายขึ้น", "ปิดคำบรรยายลง",
        "ให้แสดงคำบรรยาย", "ให้ซ่อนคำบรรยาย"
    };
#elif defined(CONFIG_LANGUAGE_VI_VN)
    // 越南文
    subtitle_rule.keywords = {
        "hiện phụ đề", "bật phụ đề", "kích hoạt phụ đề",
        "ẩn phụ đề", "tắt phụ đề", "vô hiệu hóa phụ đề",
        "phụ đề bật", "phụ đề tắt",
        "mở phụ đề", "đóng phụ đề", "cho hiện phụ đề", "cho ẩn phụ đề",
        "hiển thị phụ đề", "không hiển thị phụ đề"
    };
#else
    // 默认简体中文
    subtitle_rule.keywords = {
        "打开字幕", "开启字幕", "显示字幕",
        "关闭字幕", "隐藏字幕", "关掉字幕"
    };
#endif
    
    subtitle_rule.intent_type = IntentType::SUBTITLE_CONTROL;
    subtitle_rule.action = "ToggleSubtitle";
    subtitle_rule.device = "SubtitleControl";
    subtitle_rule.parameter_extractor = ExtractSubtitleParameters;
    AddKeywordRule(subtitle_rule);
    
    ESP_LOGI(TAG, "已加载 %d 个默认检测规则", keyword_rules_.size());
}

bool LocalIntentDetector::DetectIntent(const std::string& text, IntentResult& result) {
    if (!enabled_ || text.empty()) {
        return false;
    }
    
    std::string processed_text = PreprocessText(text);
    ESP_LOGD(TAG, "检测意图: %s -> %s", text.c_str(), processed_text.c_str());
    
    // 优先级匹配：首先检查是否包含特定设备关键词
#ifdef CONFIG_LANGUAGE_ZH_CN
    bool has_brightness_context = (text.find("亮度") != std::string::npos || text.find("屏幕") != std::string::npos);
    bool has_volume_context = (text.find("音量") != std::string::npos || text.find("声音") != std::string::npos);
    bool has_theme_context = (text.find("主题") != std::string::npos || text.find("字体") != std::string::npos);
    bool has_display_mode_context = (text.find("模式") != std::string::npos || text.find("壁纸") != std::string::npos || text.find("皮肤") != std::string::npos || text.find("表情包") != std::string::npos);
    bool has_subtitle_context = (text.find("字幕") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_EN_US)
    bool has_brightness_context = (text.find("brightness") != std::string::npos || text.find("screen") != std::string::npos);
    bool has_volume_context = (text.find("volume") != std::string::npos || text.find("sound") != std::string::npos);
    bool has_theme_context = (text.find("theme") != std::string::npos || text.find("font") != std::string::npos);
    bool has_display_mode_context = (text.find("mode") != std::string::npos || text.find("wallpaper") != std::string::npos || text.find("skin") != std::string::npos || text.find("emoticon") != std::string::npos);
    bool has_subtitle_context = (text.find("subtitle") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_JA_JP)
    bool has_brightness_context = (text.find("明るさ") != std::string::npos || text.find("画面") != std::string::npos);
    bool has_volume_context = (text.find("音量") != std::string::npos || text.find("音") != std::string::npos);
    bool has_theme_context = (text.find("テーマ") != std::string::npos || text.find("字") != std::string::npos);
    bool has_display_mode_context = (text.find("モード") != std::string::npos || text.find("壁紙") != std::string::npos || text.find("スキン") != std::string::npos || text.find("絵文字") != std::string::npos);
    bool has_subtitle_context = (text.find("字幕") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_KO_KR)
    bool has_brightness_context = (text.find("밝기") != std::string::npos || text.find("화면") != std::string::npos);
    bool has_volume_context = (text.find("볼륨") != std::string::npos || text.find("소리") != std::string::npos);
    bool has_theme_context = (text.find("테마") != std::string::npos || text.find("글꼴") != std::string::npos);
    bool has_display_mode_context = (text.find("모드") != std::string::npos || text.find("배경화면") != std::string::npos || text.find("스킨") != std::string::npos || text.find("이모티콘") != std::string::npos);
    bool has_subtitle_context = (text.find("자막") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_TH_TH)
    bool has_brightness_context = (text.find("ความสว่าง") != std::string::npos || text.find("หน้าจอ") != std::string::npos);
    bool has_volume_context = (text.find("ระดับเสียง") != std::string::npos || text.find("เสียง") != std::string::npos);
    bool has_theme_context = (text.find("ธีม") != std::string::npos || text.find("ตัวอักษร") != std::string::npos);
    bool has_display_mode_context = (text.find("โหมด") != std::string::npos || text.find("วอลเปเปอร์") != std::string::npos || text.find("สกิน") != std::string::npos || text.find("อิโมติคอน") != std::string::npos);
    bool has_subtitle_context = (text.find("คำบรรยาย") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_VI_VN)
    bool has_brightness_context = (text.find("độ sáng") != std::string::npos || text.find("màn hình") != std::string::npos);
    bool has_volume_context = (text.find("âm lượng") != std::string::npos || text.find("âm thanh") != std::string::npos);
    bool has_theme_context = (text.find("chủ đề") != std::string::npos || text.find("phông chữ") != std::string::npos);
    bool has_display_mode_context = (text.find("chế độ") != std::string::npos || text.find("hình nền") != std::string::npos || text.find("giao diện") != std::string::npos || text.find("biểu tượng cảm xúc") != std::string::npos);
    bool has_subtitle_context = (text.find("phụ đề") != std::string::npos);
#else
    // 默认简体中文
    bool has_brightness_context = (text.find("亮度") != std::string::npos || text.find("屏幕") != std::string::npos);
    bool has_volume_context = (text.find("音量") != std::string::npos || text.find("声音") != std::string::npos);
    bool has_theme_context = (text.find("主题") != std::string::npos || text.find("字体") != std::string::npos);
    bool has_display_mode_context = (text.find("模式") != std::string::npos || text.find("壁纸") != std::string::npos || text.find("皮肤") != std::string::npos || text.find("表情包") != std::string::npos);
    bool has_subtitle_context = (text.find("字幕") != std::string::npos);
#endif
    
    for (const auto& rule : keyword_rules_) {
        if (MatchKeywords(processed_text, rule.keywords)) {
            // 上下文验证：避免错误匹配
            if (rule.intent_type == IntentType::BRIGHTNESS_CONTROL && has_volume_context && !has_brightness_context) {
                ESP_LOGD(TAG, "跳过亮度规则：检测到音量上下文");
                continue;
            }
            if (rule.intent_type == IntentType::VOLUME_CONTROL && has_brightness_context && !has_volume_context) {
                ESP_LOGD(TAG, "跳过音量规则：检测到亮度上下文");
                continue;
            }
            if (rule.intent_type == IntentType::THEME_CONTROL && (has_volume_context || has_brightness_context || has_display_mode_context) && !has_theme_context) {
                ESP_LOGD(TAG, "跳过主题规则：检测到其他控制上下文");
                continue;
            }
            if (rule.intent_type == IntentType::DISPLAY_MODE_CONTROL && (has_volume_context || has_brightness_context || has_theme_context) && !has_display_mode_context) {
                ESP_LOGD(TAG, "跳过显示模式规则：检测到其他控制上下文");
                continue;
            }
            if (rule.intent_type == IntentType::SUBTITLE_CONTROL && (has_volume_context || has_brightness_context || has_display_mode_context) && !has_subtitle_context) {
                ESP_LOGD(TAG, "跳过字幕规则：检测到其他控制上下文");
                continue;
            }
            
            result.type = rule.intent_type;
            result.action = rule.action;
            result.device_name = rule.device;
            result.confidence = 0.9f; // 基础置信度
            
            // 调用参数提取器
            if (rule.parameter_extractor) {
                rule.parameter_extractor(processed_text, result);
            }
            
            ESP_LOGI(TAG, "检测到意图: %s.%s, 置信度: %.2f", 
                     result.device_name.c_str(), result.action.c_str(), result.confidence);
            
            if (intent_callback_) {
                intent_callback_(result);
            }
            return true;
        }
    }
    
    return false;
}

std::vector<IntentResult> LocalIntentDetector::DetectMultipleIntents(const std::string& text) {
    std::vector<IntentResult> results;
    
    if (!enabled_ || text.empty()) {
        return results;
    }
    
    std::string processed_text = PreprocessText(text);
    ESP_LOGD(TAG, "检测多个意图: %s -> %s", text.c_str(), processed_text.c_str());
    
    // 上下文分析
#ifdef CONFIG_LANGUAGE_ZH_CN
    bool has_brightness_context = (text.find("亮度") != std::string::npos || text.find("屏幕") != std::string::npos);
    bool has_volume_context = (text.find("音量") != std::string::npos || text.find("声音") != std::string::npos);
    bool has_theme_context = (text.find("主题") != std::string::npos || text.find("字体") != std::string::npos);
    bool has_display_mode_context = (text.find("模式") != std::string::npos || text.find("壁纸") != std::string::npos || text.find("皮肤") != std::string::npos || text.find("表情包") != std::string::npos);
    bool has_subtitle_context = (text.find("字幕") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_EN_US)
    bool has_brightness_context = (text.find("brightness") != std::string::npos || text.find("screen") != std::string::npos);
    bool has_volume_context = (text.find("volume") != std::string::npos || text.find("sound") != std::string::npos);
    bool has_theme_context = (text.find("theme") != std::string::npos || text.find("font") != std::string::npos);
    bool has_display_mode_context = (text.find("mode") != std::string::npos || text.find("wallpaper") != std::string::npos || text.find("skin") != std::string::npos || text.find("emoticon") != std::string::npos);
    bool has_subtitle_context = (text.find("subtitle") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_JA_JP)
    bool has_brightness_context = (text.find("明るさ") != std::string::npos || text.find("画面") != std::string::npos);
    bool has_volume_context = (text.find("音量") != std::string::npos || text.find("音") != std::string::npos);
    bool has_theme_context = (text.find("テーマ") != std::string::npos || text.find("字") != std::string::npos);
    bool has_display_mode_context = (text.find("モード") != std::string::npos || text.find("壁紙") != std::string::npos || text.find("スキン") != std::string::npos || text.find("絵文字") != std::string::npos);
    bool has_subtitle_context = (text.find("字幕") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_KO_KR)
    bool has_brightness_context = (text.find("밝기") != std::string::npos || text.find("화면") != std::string::npos);
    bool has_volume_context = (text.find("볼륨") != std::string::npos || text.find("소리") != std::string::npos);
    bool has_theme_context = (text.find("테마") != std::string::npos || text.find("글꼴") != std::string::npos);
    bool has_display_mode_context = (text.find("모드") != std::string::npos || text.find("배경화면") != std::string::npos || text.find("스킨") != std::string::npos || text.find("이모티콘") != std::string::npos);
    bool has_subtitle_context = (text.find("자막") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_TH_TH)
    bool has_brightness_context = (text.find("ความสว่าง") != std::string::npos || text.find("หน้าจอ") != std::string::npos);
    bool has_volume_context = (text.find("ระดับเสียง") != std::string::npos || text.find("เสียง") != std::string::npos);
    bool has_theme_context = (text.find("ธีม") != std::string::npos || text.find("ตัวอักษร") != std::string::npos);
    bool has_display_mode_context = (text.find("โหมด") != std::string::npos || text.find("วอลเปเปอร์") != std::string::npos || text.find("สกิน") != std::string::npos || text.find("อิโมติคอน") != std::string::npos);
    bool has_subtitle_context = (text.find("คำบรรยาย") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_VI_VN)
    bool has_brightness_context = (text.find("độ sáng") != std::string::npos || text.find("màn hình") != std::string::npos);
    bool has_volume_context = (text.find("âm lượng") != std::string::npos || text.find("âm thanh") != std::string::npos);
    bool has_theme_context = (text.find("chủ đề") != std::string::npos || text.find("phông chữ") != std::string::npos);
    bool has_display_mode_context = (text.find("chế độ") != std::string::npos || text.find("hình nền") != std::string::npos || text.find("giao diện") != std::string::npos || text.find("biểu tượng cảm xúc") != std::string::npos);
    bool has_subtitle_context = (text.find("phụ đề") != std::string::npos);
#else
    // 默认简体中文
    bool has_brightness_context = (text.find("亮度") != std::string::npos || text.find("屏幕") != std::string::npos);
    bool has_volume_context = (text.find("音量") != std::string::npos || text.find("声音") != std::string::npos);
    bool has_theme_context = (text.find("主题") != std::string::npos || text.find("字体") != std::string::npos);
    bool has_display_mode_context = (text.find("模式") != std::string::npos || text.find("壁纸") != std::string::npos || text.find("皮肤") != std::string::npos || text.find("表情包") != std::string::npos);
    bool has_subtitle_context = (text.find("字幕") != std::string::npos);
#endif
    
    // 跟踪已处理的意图类型，避免重复
    std::set<IntentType> detected_types;
    
    for (const auto& rule : keyword_rules_) {
        if (MatchKeywords(processed_text, rule.keywords)) {
            // 避免同一类型的重复检测
            if (detected_types.count(rule.intent_type) > 0) {
                ESP_LOGD(TAG, "跳过重复意图类型: %d", (int)rule.intent_type);
                continue;
            }
            
            // 上下文验证：避免错误匹配
            if (rule.intent_type == IntentType::BRIGHTNESS_CONTROL && has_volume_context && !has_brightness_context) {
                ESP_LOGD(TAG, "跳过亮度规则：检测到音量上下文");
                continue;
            }
            if (rule.intent_type == IntentType::VOLUME_CONTROL && has_brightness_context && !has_volume_context) {
                ESP_LOGD(TAG, "跳过音量规则：检测到亮度上下文");
                continue;
            }
            if (rule.intent_type == IntentType::THEME_CONTROL && (has_volume_context || has_brightness_context || has_display_mode_context) && !has_theme_context) {
                ESP_LOGD(TAG, "跳过主题规则：检测到其他控制上下文");
                continue;
            }
            if (rule.intent_type == IntentType::DISPLAY_MODE_CONTROL && (has_volume_context || has_brightness_context || has_theme_context) && !has_display_mode_context) {
                ESP_LOGD(TAG, "跳过显示模式规则：检测到其他控制上下文");
                continue;
            }
            if (rule.intent_type == IntentType::SUBTITLE_CONTROL && (has_volume_context || has_brightness_context || has_display_mode_context) && !has_subtitle_context) {
                ESP_LOGD(TAG, "跳过字幕规则：检测到其他控制上下文");
                continue;
            }
            if (rule.intent_type == IntentType::SUBTITLE_CONTROL && (has_volume_context || has_brightness_context || has_display_mode_context) && !has_subtitle_context) {
                ESP_LOGD(TAG, "跳过字幕规则：检测到其他控制上下文");
                continue;
            }
            
            IntentResult result;
            result.type = rule.intent_type;
            result.action = rule.action;
            result.device_name = rule.device;
            result.confidence = 0.9f; // 基础置信度
            
            // 调用参数提取器
            if (rule.parameter_extractor) {
                rule.parameter_extractor(processed_text, result);
            }
            
            results.push_back(result);
            detected_types.insert(rule.intent_type);
            
            ESP_LOGI(TAG, "检测到意图 %zu: %s.%s, 置信度: %.2f", 
                     results.size(), result.device_name.c_str(), result.action.c_str(), result.confidence);
        }
    }
    
    ESP_LOGI(TAG, "总共检测到 %zu 个意图", results.size());
    return results;
}

void LocalIntentDetector::FeedAudioData(const std::vector<int16_t>& audio_data) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(audio_buffer_mutex_);
    audio_buffer_.push_back(audio_data);
    
    // 保持缓冲区大小在合理范围内（约1秒的数据）
    while (audio_buffer_.size() > 50) { // 假设20ms一帧，50帧约1秒
        audio_buffer_.pop_front();
    }
}

void LocalIntentDetector::OnIntentDetected(std::function<void(const IntentResult& result)> callback) {
    intent_callback_ = callback;
}

void LocalIntentDetector::AddKeywordRule(const KeywordRule& rule) {
    keyword_rules_.push_back(rule);
}

std::string LocalIntentDetector::PreprocessText(const std::string& text) {
    std::string result = text;
    
    // 转换为小写
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    
    // 移除标点符号和空格
    result.erase(std::remove_if(result.begin(), result.end(), 
                 [](char c) { return std::ispunct(c) || std::isspace(c); }), 
                 result.end());
    
    return result;
}

bool LocalIntentDetector::MatchKeywords(const std::string& text, const std::vector<std::string>& keywords) {
    for (const auto& keyword : keywords) {
        std::string processed_keyword = PreprocessText(keyword);
        if (text.find(processed_keyword) != std::string::npos) {
            ESP_LOGD(TAG, "关键词匹配: %s 在 %s 中找到", processed_keyword.c_str(), text.c_str());
            return true;
        }
    }
    return false;
}

int LocalIntentDetector::ExtractNumberStatic(const std::string& text) {
    // 简化的数字提取，避免使用regex节省栈空间
    
    // 查找阿拉伯数字 - 使用简单的字符扫描代替正则表达式
    std::string current_number;
    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        if (c >= '0' && c <= '9') {
            current_number += c;
        } else {
            if (!current_number.empty() && current_number.length() <= 3) {
                int num = std::atoi(current_number.c_str());
                if (num >= 0 && num <= 100) { // 音量范围限制
                    return num;
                }
            }
            current_number.clear();
        }
    }
    
    // 检查最后的数字
    if (!current_number.empty() && current_number.length() <= 3) {
        int num = std::atoi(current_number.c_str());
        if (num >= 0 && num <= 100) {
            return num;
        }
    }
    
    // 使用完整的中文数字表，按长度优先匹配
    static const std::vector<std::pair<std::string, int>> chinese_numbers_complete = {
        // 三字符数字（最高优先级）
        {"九十九", 99}, {"九十八", 98}, {"九十七", 97}, {"九十六", 96}, {"九十五", 95},
        {"九十四", 94}, {"九十三", 93}, {"九十二", 92}, {"九十一", 91},
        {"八十九", 89}, {"八十八", 88}, {"八十七", 87}, {"八十六", 86}, {"八十五", 85},
        {"八十四", 84}, {"八十三", 83}, {"八十二", 82}, {"八十一", 81},
        {"七十九", 79}, {"七十八", 78}, {"七十七", 77}, {"七十六", 76}, {"七十五", 75},
        {"七十四", 74}, {"七十三", 73}, {"七十二", 72}, {"七十一", 71},
        {"六十九", 69}, {"六十八", 68}, {"六十七", 67}, {"六十六", 66}, {"六十五", 65},
        {"六十四", 64}, {"六十三", 63}, {"六十二", 62}, {"六十一", 61},
        {"五十九", 59}, {"五十八", 58}, {"五十七", 57}, {"五十六", 56}, {"五十五", 55},
        {"五十四", 54}, {"五十三", 53}, {"五十二", 52}, {"五十一", 51},
        {"四十九", 49}, {"四十八", 48}, {"四十七", 47}, {"四十六", 46}, {"四十五", 45},
        {"四十四", 44}, {"四十三", 43}, {"四十二", 42}, {"四十一", 41},
        {"三十九", 39}, {"三十八", 38}, {"三十七", 37}, {"三十六", 36}, {"三十五", 35},
        {"三十四", 34}, {"三十三", 33}, {"三十二", 32}, {"三十一", 31},
        {"二十九", 29}, {"二十八", 28}, {"二十七", 27}, {"二十六", 26}, {"二十五", 25},
        {"二十四", 24}, {"二十三", 23}, {"二十二", 22}, {"二十一", 21},
        // 两字符数字
        {"一十", 10}, {"十一", 11}, {"十二", 12}, {"十三", 13}, {"十四", 14}, {"十五", 15},
        {"十六", 16}, {"十七", 17}, {"十八", 18}, {"十九", 19}, {"二十", 20},
        {"三十", 30}, {"四十", 40}, {"五十", 50}, {"六十", 60}, {"七十", 70},
        {"八十", 80}, {"九十", 90}, {"一百", 100},
        // 单字符数字（最低优先级）
        {"十", 10}, {"一", 1}, {"二", 2}, {"三", 3}, {"四", 4}, {"五", 5},
        {"六", 6}, {"七", 7}, {"八", 8}, {"九", 9}, {"零", 0}
    };
    
    for (const auto& pair : chinese_numbers_complete) {
        if (text.find(pair.first) != std::string::npos) {
            return pair.second;
        }
    }
    
    return -1;
}

int LocalIntentDetector::ExtractNumberWithContext(const std::string& text, const std::vector<std::string>& context_keywords) {
    if (text.empty() || context_keywords.empty()) {
        return ExtractNumberStatic(text); // 回退到静态提取
    }
    
    ESP_LOGI(TAG, "上下文数字提取开始: '%s'", text.c_str());
    
    // 找到所有数字及其位置
    std::vector<std::pair<int, size_t>> numbers_with_positions;
    
    // 查找阿拉伯数字
    std::string current_number;
    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        if (c >= '0' && c <= '9') {
            current_number += c;
        } else {
            if (!current_number.empty() && current_number.length() <= 3) {
                int num = std::atoi(current_number.c_str());
                if (num >= 0 && num <= 100) {
                    // 记录数字的结束位置
                    numbers_with_positions.push_back({num, i});
                }
            }
            current_number.clear();
        }
    }
    
    // 检查最后的数字
    if (!current_number.empty() && current_number.length() <= 3) {
        int num = std::atoi(current_number.c_str());
        if (num >= 0 && num <= 100) {
            numbers_with_positions.push_back({num, text.length()});
        }
    }
    
    // 完整的0-100中文数字表 - 按长度优先排序
    static const std::vector<std::pair<std::string, int>> chinese_numbers_complete = {
        // 三字符数字
        {"九十九", 99}, {"九十八", 98}, {"九十七", 97}, {"九十六", 96}, {"九十五", 95},
        {"九十四", 94}, {"九十三", 93}, {"九十二", 92}, {"九十一", 91},
        {"八十九", 89}, {"八十八", 88}, {"八十七", 87}, {"八十六", 86}, {"八十五", 85},
        {"八十四", 84}, {"八十三", 83}, {"八十二", 82}, {"八十一", 81},
        {"七十九", 79}, {"七十八", 78}, {"七十七", 77}, {"七十六", 76}, {"七十五", 75},
        {"七十四", 74}, {"七十三", 73}, {"七十二", 72}, {"七十一", 71},
        {"六十九", 69}, {"六十八", 68}, {"六十七", 67}, {"六十六", 66}, {"六十五", 65},
        {"六十四", 64}, {"六十三", 63}, {"六十二", 62}, {"六十一", 61},
        {"五十九", 59}, {"五十八", 58}, {"五十七", 57}, {"五十六", 56}, {"五十五", 55},
        {"五十四", 54}, {"五十三", 53}, {"五十二", 52}, {"五十一", 51},
        {"四十九", 49}, {"四十八", 48}, {"四十七", 47}, {"四十六", 46}, {"四十五", 45},
        {"四十四", 44}, {"四十三", 43}, {"四十二", 42}, {"四十一", 41},
        {"三十九", 39}, {"三十八", 38}, {"三十七", 37}, {"三十六", 36}, {"三十五", 35},
        {"三十四", 34}, {"三十三", 33}, {"三十二", 32}, {"三十一", 31},
        {"二十九", 29}, {"二十八", 28}, {"二十七", 27}, {"二十六", 26}, {"二十五", 25},
        {"二十四", 24}, {"二十三", 23}, {"二十二", 22}, {"二十一", 21},
        // 两字符数字
        {"一十", 10}, {"十一", 11}, {"十二", 12}, {"十三", 13}, {"十四", 14}, {"十五", 15},
        {"十六", 16}, {"十七", 17}, {"十八", 18}, {"十九", 19}, {"二十", 20},
        {"三十", 30}, {"四十", 40}, {"五十", 50}, {"六十", 60}, {"七十", 70},
        {"八十", 80}, {"九十", 90}, {"一百", 100},
        // 单字符数字（最低优先级）
        {"十", 10}, {"一", 1}, {"二", 2}, {"三", 3}, {"四", 4}, {"五", 5},
        {"六", 6}, {"七", 7}, {"八", 8}, {"九", 9}, {"零", 0}
    };
    
    // 用字符串替换的方式，优先处理长的数字，避免重叠
    std::string text_copy = text;
    std::vector<std::pair<int, size_t>> temp_numbers;
    
    for (const auto& pair : chinese_numbers_complete) {
        size_t pos = 0;
        while ((pos = text_copy.find(pair.first, pos)) != std::string::npos) {
            if (pair.second <= 100) {
                temp_numbers.push_back({pair.second, pos + pair.first.length()});
                ESP_LOGI(TAG, "找到中文数字: '%s' = %d, 原位置: %zu", 
                         pair.first.c_str(), pair.second, pos);
                
                // 用占位符替换已匹配的数字，避免重复匹配
                std::string placeholder(pair.first.length(), 'X');
                text_copy.replace(pos, pair.first.length(), placeholder);
            }
            pos += pair.first.length();
        }
    }
    
    // 将找到的中文数字添加到总列表
    for (const auto& num : temp_numbers) {
        numbers_with_positions.push_back(num);
    }
    
    if (numbers_with_positions.empty()) {
        ESP_LOGI(TAG, "未找到数字");
        return -1;
    }
    
    ESP_LOGI(TAG, "找到 %zu 个数字:", numbers_with_positions.size());
    for (size_t i = 0; i < numbers_with_positions.size(); ++i) {
        ESP_LOGI(TAG, "  数字 %zu: %d (位置: %zu)", i+1, numbers_with_positions[i].first, numbers_with_positions[i].second);
    }
    
    // 找到最近的关键词位置
    size_t closest_keyword_pos = std::string::npos;
    std::string matched_keyword;
    for (const auto& keyword : context_keywords) {
        size_t pos = text.find(keyword);
        if (pos != std::string::npos) {
            if (closest_keyword_pos == std::string::npos || pos < closest_keyword_pos) {
                closest_keyword_pos = pos;
                matched_keyword = keyword;
            }
        }
    }
    
    if (closest_keyword_pos != std::string::npos) {
        ESP_LOGI(TAG, "找到上下文关键词: '%s' 在位置 %zu", matched_keyword.c_str(), closest_keyword_pos);
    }
    
    if (closest_keyword_pos == std::string::npos) {
        ESP_LOGI(TAG, "未找到上下文关键词，使用第一个数字: %d", numbers_with_positions[0].first);
        return numbers_with_positions[0].first;
    }
    
    // 改进的数字选择算法：优先选择关键词之后最近的数字
    int best_number = numbers_with_positions[0].first;
    size_t min_score = std::numeric_limits<size_t>::max();
    
    ESP_LOGI(TAG, "关键词 '%s' 位置: %zu", matched_keyword.c_str(), closest_keyword_pos);
    
    for (const auto& num_pos : numbers_with_positions) {
        size_t distance = std::abs((long)num_pos.second - (long)closest_keyword_pos);
        
        // 计算评分：优先考虑在关键词之后的数字
        size_t score;
        if (num_pos.second > closest_keyword_pos) {
            // 数字在关键词之后：使用实际距离
            score = distance;
            ESP_LOGI(TAG, "数字 %d (位置:%zu) 在关键词之后，距离:%zu, 评分:%zu", 
                     num_pos.first, num_pos.second, distance, score);
        } else {
            // 数字在关键词之前：大幅增加评分（降低优先级）
            score = distance + 1000;
            ESP_LOGI(TAG, "数字 %d (位置:%zu) 在关键词之前，距离:%zu, 评分:%zu (惩罚)", 
                     num_pos.first, num_pos.second, distance, score);
        }
        
        if (score < min_score) {
            min_score = score;
            best_number = num_pos.first;
        }
    }
    
    ESP_LOGI(TAG, "上下文数字提取结果: 选择数字 %d (最佳评分:%zu), 总共找到 %zu 个数字", 
             best_number, min_score, numbers_with_positions.size());
    return best_number;
}

int LocalIntentDetector::ChineseNumberToInt(const std::string& chinese_num) {
    // 简化的中文数字转换实现
    if (chinese_num == "十") return 10;
    if (chinese_num == "二十") return 20;
    if (chinese_num == "三十") return 30;
    if (chinese_num == "四十") return 40;
    if (chinese_num == "五十") return 50;
    if (chinese_num == "六十") return 60;
    if (chinese_num == "七十") return 70;
    if (chinese_num == "八十") return 80;
    if (chinese_num == "九十") return 90;
    if (chinese_num == "一百") return 100;
    
    return -1;
}

void LocalIntentDetector::ExtractVolumeParameters(const std::string& text, IntentResult& result) {
    // 首先检查特殊情况 - 最大音量
#ifdef CONFIG_LANGUAGE_ZH_CN
    bool is_max = (text.find("最大") != std::string::npos || text.find("最响") != std::string::npos);
    bool is_min = (text.find("最小") != std::string::npos || text.find("静音") != std::string::npos);
    bool is_inc_small = (text.find("大一点") != std::string::npos || text.find("大一些") != std::string::npos);
    bool is_dec_small = (text.find("小一点") != std::string::npos || text.find("小一些") != std::string::npos);
    bool is_inc = (text.find("大") != std::string::npos || text.find("高") != std::string::npos);
    bool is_dec = (text.find("小") != std::string::npos || text.find("低") != std::string::npos);
    std::vector<std::string> context_keywords = {"音量", "声音"};
#elif defined(CONFIG_LANGUAGE_EN_US)
    bool is_max = (text.find("maximum") != std::string::npos || text.find("loudest") != std::string::npos || text.find("max") != std::string::npos);
    bool is_min = (text.find("minimum") != std::string::npos || text.find("mute") != std::string::npos || text.find("silence") != std::string::npos);
    bool is_inc_small = (text.find("a bit louder") != std::string::npos || text.find("little louder") != std::string::npos);
    bool is_dec_small = (text.find("a bit quieter") != std::string::npos || text.find("little quieter") != std::string::npos);
    bool is_inc = (text.find("louder") != std::string::npos || text.find("up") != std::string::npos || text.find("increase") != std::string::npos);
    bool is_dec = (text.find("quieter") != std::string::npos || text.find("down") != std::string::npos || text.find("decrease") != std::string::npos);
    std::vector<std::string> context_keywords = {"volume", "sound"};
#elif defined(CONFIG_LANGUAGE_JA_JP)
    bool is_max = (text.find("最大") != std::string::npos || text.find("最大音量") != std::string::npos);
    bool is_min = (text.find("最小") != std::string::npos || text.find("ミュート") != std::string::npos || text.find("消音") != std::string::npos);
    bool is_inc_small = (text.find("少し大きく") != std::string::npos);
    bool is_dec_small = (text.find("少し小さく") != std::string::npos);
    bool is_inc = (text.find("大きく") != std::string::npos || text.find("上げ") != std::string::npos);
    bool is_dec = (text.find("小さく") != std::string::npos || text.find("下げ") != std::string::npos);
    std::vector<std::string> context_keywords = {"音量", "音"};
#elif defined(CONFIG_LANGUAGE_KO_KR)
    bool is_max = (text.find("최대") != std::string::npos || text.find("가장 크게") != std::string::npos);
    bool is_min = (text.find("최소") != std::string::npos || text.find("음소거") != std::string::npos || text.find("무음") != std::string::npos);
    bool is_inc_small = (text.find("조금 크게") != std::string::npos);
    bool is_dec_small = (text.find("조금 작게") != std::string::npos);
    bool is_inc = (text.find("크게") != std::string::npos || text.find("올리") != std::string::npos);
    bool is_dec = (text.find("작게") != std::string::npos || text.find("내리") != std::string::npos);
    std::vector<std::string> context_keywords = {"볼륨", "소리"};
#elif defined(CONFIG_LANGUAGE_TH_TH)
    bool is_max = (text.find("สูงสุด") != std::string::npos || text.find("ดังที่สุด") != std::string::npos);
    bool is_min = (text.find("ต่ำสุด") != std::string::npos || text.find("ปิดเสียง") != std::string::npos || text.find("เงียบ") != std::string::npos);
    bool is_inc_small = (text.find("ดังขึ้นนิดหน่อย") != std::string::npos);
    bool is_dec_small = (text.find("เบาลงนิดหน่อย") != std::string::npos);
    bool is_inc = (text.find("ดัง") != std::string::npos || text.find("เพิ่ม") != std::string::npos);
    bool is_dec = (text.find("เบา") != std::string::npos || text.find("ลด") != std::string::npos);
    std::vector<std::string> context_keywords = {"ระดับเสียง", "เสียง"};
#elif defined(CONFIG_LANGUAGE_VI_VN)
    bool is_max = (text.find("tối đa") != std::string::npos || text.find("to nhất") != std::string::npos);
    bool is_min = (text.find("tối thiểu") != std::string::npos || text.find("tắt tiếng") != std::string::npos || text.find("im lặng") != std::string::npos);
    bool is_inc_small = (text.find("to lên một chút") != std::string::npos);
    bool is_dec_small = (text.find("nhỏ đi một chút") != std::string::npos);
    bool is_inc = (text.find("to hơn") != std::string::npos || text.find("tăng") != std::string::npos);
    bool is_dec = (text.find("nhỏ hơn") != std::string::npos || text.find("giảm") != std::string::npos);
    std::vector<std::string> context_keywords = {"âm lượng", "âm thanh"};
#else
    // 默认简体中文
    bool is_max = (text.find("最大") != std::string::npos || text.find("最响") != std::string::npos);
    bool is_min = (text.find("最小") != std::string::npos || text.find("静音") != std::string::npos);
    bool is_inc_small = (text.find("大一点") != std::string::npos || text.find("大一些") != std::string::npos);
    bool is_dec_small = (text.find("小一点") != std::string::npos || text.find("小一些") != std::string::npos);
    bool is_inc = (text.find("大") != std::string::npos || text.find("高") != std::string::npos);
    bool is_dec = (text.find("小") != std::string::npos || text.find("低") != std::string::npos);
    std::vector<std::string> context_keywords = {"音量", "声音"};
#endif
    
    if (is_max) {
        result.parameters["volume"] = "100";
        result.confidence = 0.98f;
        ESP_LOGI(TAG, "检测到音量最大请求: 100");
        return;
    }
    
    if (is_min) {
        result.parameters["volume"] = "0";
        result.confidence = 0.98f;
        ESP_LOGI(TAG, "检测到音量最小/静音请求: 0");
        return;
    }
    
    if (is_inc_small) {
        result.parameters["relative"] = "increase_10";
        result.confidence = 0.9f;
        ESP_LOGI(TAG, "检测到音量大一点请求: +10");
        return;
    }
    
    if (is_dec_small) {
        result.parameters["relative"] = "decrease_10";
        result.confidence = 0.9f;
        ESP_LOGI(TAG, "检测到音量小一点请求: -10");
        return;
    }
    
    // 使用上下文相关的数字提取
    int volume = ExtractNumberWithContext(text, context_keywords);
    
    if (volume >= 0 && volume <= 100) {
        result.parameters["volume"] = std::to_string(volume);
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "提取音量参数: %d", volume);
    } else {
        // 处理其他相对调节
        if (is_inc) {
            result.parameters["relative"] = "increase";
            result.confidence = 0.8f;
        } else if (is_dec) {
            result.parameters["relative"] = "decrease";
            result.confidence = 0.8f;
        }
    }
}

void LocalIntentDetector::ExtractBrightnessParameters(const std::string& text, IntentResult& result) {
    // 首先检查特殊情况 - 最大亮度
#ifdef CONFIG_LANGUAGE_ZH_CN
    bool is_max = (text.find("最大") != std::string::npos || text.find("最亮") != std::string::npos);
    bool is_min = (text.find("最小") != std::string::npos || text.find("最暗") != std::string::npos);
    bool is_inc_small = (text.find("大一点") != std::string::npos || text.find("亮一点") != std::string::npos || text.find("大一些") != std::string::npos);
    bool is_dec_small = (text.find("小一点") != std::string::npos || text.find("暗一点") != std::string::npos || text.find("小一些") != std::string::npos);
    bool is_inc = (text.find("亮") != std::string::npos || text.find("调亮") != std::string::npos || text.find("变亮") != std::string::npos);
    bool is_dec = (text.find("暗") != std::string::npos || text.find("调暗") != std::string::npos || text.find("变暗") != std::string::npos);
    std::vector<std::string> context_keywords = {"亮度", "屏幕"};
#elif defined(CONFIG_LANGUAGE_EN_US)
    bool is_max = (text.find("maximum") != std::string::npos || text.find("brightest") != std::string::npos || text.find("max") != std::string::npos);
    bool is_min = (text.find("minimum") != std::string::npos || text.find("dimmest") != std::string::npos || text.find("min") != std::string::npos);
    bool is_inc_small = (text.find("a bit brighter") != std::string::npos || text.find("little brighter") != std::string::npos);
    bool is_dec_small = (text.find("a bit darker") != std::string::npos || text.find("little darker") != std::string::npos);
    bool is_inc = (text.find("brighter") != std::string::npos || text.find("brighten") != std::string::npos || text.find("increase") != std::string::npos);
    bool is_dec = (text.find("darker") != std::string::npos || text.find("dim") != std::string::npos || text.find("decrease") != std::string::npos);
    std::vector<std::string> context_keywords = {"brightness", "screen"};
#elif defined(CONFIG_LANGUAGE_JA_JP)
    bool is_max = (text.find("最大") != std::string::npos || text.find("最大限") != std::string::npos);
    bool is_min = (text.find("最小") != std::string::npos || text.find("最小限") != std::string::npos);
    bool is_inc_small = (text.find("少し明るく") != std::string::npos);
    bool is_dec_small = (text.find("少し暗く") != std::string::npos);
    bool is_inc = (text.find("明るく") != std::string::npos || text.find("上げ") != std::string::npos);
    bool is_dec = (text.find("暗く") != std::string::npos || text.find("下げ") != std::string::npos);
    std::vector<std::string> context_keywords = {"明るさ", "画面"};
#elif defined(CONFIG_LANGUAGE_KO_KR)
    bool is_max = (text.find("최대") != std::string::npos || text.find("가장 밝게") != std::string::npos);
    bool is_min = (text.find("최소") != std::string::npos || text.find("가장 어둡게") != std::string::npos);
    bool is_inc_small = (text.find("조금 밝게") != std::string::npos);
    bool is_dec_small = (text.find("조금 어둡게") != std::string::npos);
    bool is_inc = (text.find("밝게") != std::string::npos || text.find("올리") != std::string::npos);
    bool is_dec = (text.find("어둡게") != std::string::npos || text.find("내리") != std::string::npos);
    std::vector<std::string> context_keywords = {"밝기", "화면"};
#elif defined(CONFIG_LANGUAGE_TH_TH)
    bool is_max = (text.find("สูงสุด") != std::string::npos || text.find("สว่างที่สุด") != std::string::npos);
    bool is_min = (text.find("ต่ำสุด") != std::string::npos || text.find("มืดที่สุด") != std::string::npos);
    bool is_inc_small = (text.find("สว่างขึ้นนิดหน่อย") != std::string::npos);
    bool is_dec_small = (text.find("มืดลงนิดหน่อย") != std::string::npos);
    bool is_inc = (text.find("สว่าง") != std::string::npos || text.find("เพิ่ม") != std::string::npos);
    bool is_dec = (text.find("มืด") != std::string::npos || text.find("ลด") != std::string::npos);
    std::vector<std::string> context_keywords = {"ความสว่าง", "หน้าจอ"};
#elif defined(CONFIG_LANGUAGE_VI_VN)
    bool is_max = (text.find("tối đa") != std::string::npos || text.find("sáng nhất") != std::string::npos);
    bool is_min = (text.find("tối thiểu") != std::string::npos || text.find("tối nhất") != std::string::npos);
    bool is_inc_small = (text.find("sáng lên một chút") != std::string::npos);
    bool is_dec_small = (text.find("tối đi một chút") != std::string::npos);
    bool is_inc = (text.find("sáng hơn") != std::string::npos || text.find("tăng") != std::string::npos || text.find("làm sáng") != std::string::npos);
    bool is_dec = (text.find("tối hơn") != std::string::npos || text.find("giảm") != std::string::npos || text.find("làm tối") != std::string::npos);
    std::vector<std::string> context_keywords = {"độ sáng", "màn hình"};
#else
    // 默认简体中文
    bool is_max = (text.find("最大") != std::string::npos || text.find("最亮") != std::string::npos);
    bool is_min = (text.find("最小") != std::string::npos || text.find("最暗") != std::string::npos);
    bool is_inc_small = (text.find("大一点") != std::string::npos || text.find("亮一点") != std::string::npos || text.find("大一些") != std::string::npos);
    bool is_dec_small = (text.find("小一点") != std::string::npos || text.find("暗一点") != std::string::npos || text.find("小一些") != std::string::npos);
    bool is_inc = (text.find("亮") != std::string::npos || text.find("调亮") != std::string::npos || text.find("变亮") != std::string::npos);
    bool is_dec = (text.find("暗") != std::string::npos || text.find("调暗") != std::string::npos || text.find("变暗") != std::string::npos);
    std::vector<std::string> context_keywords = {"亮度", "屏幕"};
#endif
    
    if (is_max) {
        result.parameters["brightness"] = "100";
        result.confidence = 0.98f;
        ESP_LOGI(TAG, "检测到亮度最大请求: 100");
        return;
    }
    
    if (is_min) {
        result.parameters["brightness"] = "0";
        result.confidence = 0.98f;
        ESP_LOGI(TAG, "检测到亮度最小请求: 0");
        return;
    }
    
    if (is_inc_small) {
        result.parameters["relative"] = "increase_10";
        result.confidence = 0.9f;
        ESP_LOGI(TAG, "检测到亮度大一点请求: +10");
        return;
    }
    
    if (is_dec_small) {
        result.parameters["relative"] = "decrease_10";
        result.confidence = 0.9f;
        ESP_LOGI(TAG, "检测到亮度小一点请求: -10");
        return;
    }
    
    // 使用上下文相关的数字提取
    int brightness = ExtractNumberWithContext(text, context_keywords);
    
    if (brightness >= 0 && brightness <= 100) {
        result.parameters["brightness"] = std::to_string(brightness);
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "提取亮度参数: %d", brightness);
    } else {
        // 处理其他相对调节
        if (is_inc) {
            result.parameters["relative"] = "increase";
            result.confidence = 0.85f;
            ESP_LOGI(TAG, "检测到亮度调亮请求");
        } else if (is_dec) {
            result.parameters["relative"] = "decrease";
            result.confidence = 0.85f;
            ESP_LOGI(TAG, "检测到亮度调暗请求");
        } else {
            // 没有找到具体数值或相对调节，使用默认亮度
            result.parameters["brightness"] = "75";
            result.confidence = 0.7f;
            ESP_LOGI(TAG, "使用默认亮度: 75");
        }
    }
}

void LocalIntentDetector::ExtractThemeParameters(const std::string& text, IntentResult& result) {
    // 检查暗色主题关键词
#ifdef CONFIG_LANGUAGE_ZH_CN
    bool is_dark = (text.find("黑色主题") != std::string::npos || text.find("黑夜模式") != std::string::npos ||
                    text.find("黑色字体") != std::string::npos || text.find("黑色字幕") != std::string::npos ||
                    text.find("暗色主题") != std::string::npos);
    bool is_light = (text.find("白色主题") != std::string::npos || text.find("白天模式") != std::string::npos ||
                     text.find("白色字体") != std::string::npos || text.find("白色字幕") != std::string::npos ||
                     text.find("亮色主题") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_EN_US)
    bool is_dark = (text.find("black theme") != std::string::npos || text.find("night mode") != std::string::npos ||
                    text.find("dark theme") != std::string::npos || text.find("dark mode") != std::string::npos);
    bool is_light = (text.find("white theme") != std::string::npos || text.find("day mode") != std::string::npos ||
                     text.find("light theme") != std::string::npos || text.find("light mode") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_JA_JP)
    bool is_dark = (text.find("黒いテーマ") != std::string::npos || text.find("夜間モード") != std::string::npos ||
                    text.find("ダークテーマ") != std::string::npos || text.find("暗いモード") != std::string::npos);
    bool is_light = (text.find("白いテーマ") != std::string::npos || text.find("昼間モード") != std::string::npos ||
                     text.find("ライトテーマ") != std::string::npos || text.find("明るいモード") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_KO_KR)
    bool is_dark = (text.find("검은색 테마") != std::string::npos || text.find("밤 모드") != std::string::npos ||
                    text.find("다크 테마") != std::string::npos || text.find("어두운 모드") != std::string::npos);
    bool is_light = (text.find("흰색 테마") != std::string::npos || text.find("낮 모드") != std::string::npos ||
                     text.find("라이트 테마") != std::string::npos || text.find("밝은 모드") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_TH_TH)
    bool is_dark = (text.find("ธีมสีดำ") != std::string::npos || text.find("โหมดกลางคืน") != std::string::npos ||
                    text.find("ธีมมืด") != std::string::npos);
    bool is_light = (text.find("ธีมสีขาว") != std::string::npos || text.find("โหมดกลางวัน") != std::string::npos ||
                     text.find("ธีมสว่าง") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_VI_VN)
    bool is_dark = (text.find("chủ đề đen") != std::string::npos || text.find("chế độ đêm") != std::string::npos ||
                    text.find("chủ đề tối") != std::string::npos || text.find("chế độ tối") != std::string::npos);
    bool is_light = (text.find("chủ đề trắng") != std::string::npos || text.find("chế độ ngày") != std::string::npos ||
                     text.find("chủ đề sáng") != std::string::npos || text.find("chế độ sáng") != std::string::npos);
#else
    // 默认简体中文
    bool is_dark = (text.find("黑色主题") != std::string::npos || text.find("黑夜模式") != std::string::npos ||
                    text.find("黑色字体") != std::string::npos || text.find("黑色字幕") != std::string::npos ||
                    text.find("暗色主题") != std::string::npos);
    bool is_light = (text.find("白色主题") != std::string::npos || text.find("白天模式") != std::string::npos ||
                     text.find("白色字体") != std::string::npos || text.find("白色字幕") != std::string::npos ||
                     text.find("亮色主题") != std::string::npos);
#endif
    
    if (is_dark) {
        result.parameters["theme_name"] = "dark";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到暗色主题请求: dark");
        return;
    }
    
    if (is_light) {
        result.parameters["theme_name"] = "light";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到亮色主题请求: light");
        return;
    }
    
    // 默认情况：使用暗色主题
    result.parameters["theme_name"] = "dark";
    result.confidence = 0.7f;
    ESP_LOGI(TAG, "使用默认主题: dark");
}

void LocalIntentDetector::ExtractDisplayModeParameters(const std::string& text, IntentResult& result) {
    // 检查表情包模式关键词（优先级最高）
#ifdef CONFIG_LANGUAGE_ZH_CN
    bool is_emoticon = (text.find("表情包模式") != std::string::npos || text.find("表情模式") != std::string::npos ||
                        text.find("情绪模式") != std::string::npos || text.find("切换到表情包") != std::string::npos ||
                        text.find("emoji模式") != std::string::npos);
    bool is_static = (text.find("静态模式") != std::string::npos || text.find("静态壁纸") != std::string::npos ||
                      text.find("静态皮肤") != std::string::npos);
    bool is_animated = (text.find("动态模式") != std::string::npos || text.find("动态壁纸") != std::string::npos ||
                        text.find("动态皮肤") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_EN_US)
    bool is_emoticon = (text.find("emoticon mode") != std::string::npos || text.find("emoji mode") != std::string::npos ||
                        text.find("emotion mode") != std::string::npos || text.find("expression mode") != std::string::npos);
    bool is_static = (text.find("static mode") != std::string::npos || text.find("static wallpaper") != std::string::npos ||
                      text.find("static skin") != std::string::npos);
    bool is_animated = (text.find("animated mode") != std::string::npos || text.find("animated wallpaper") != std::string::npos ||
                        text.find("animated skin") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_JA_JP)
    bool is_emoticon = (text.find("絵文字モード") != std::string::npos || text.find("エモーティコンモード") != std::string::npos ||
                        text.find("感情モード") != std::string::npos || text.find("表情モード") != std::string::npos);
    bool is_static = (text.find("静的モード") != std::string::npos || text.find("静的壁紙") != std::string::npos ||
                      text.find("静的スキン") != std::string::npos);
    bool is_animated = (text.find("動的モード") != std::string::npos || text.find("動的壁紙") != std::string::npos ||
                        text.find("動的スキン") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_KO_KR)
    bool is_emoticon = (text.find("이모티콘 모드") != std::string::npos || text.find("이모지 모드") != std::string::npos ||
                        text.find("감정 모드") != std::string::npos || text.find("표정 모드") != std::string::npos);
    bool is_static = (text.find("정적 모드") != std::string::npos || text.find("정적 배경화면") != std::string::npos ||
                      text.find("정적 스킨") != std::string::npos);
    bool is_animated = (text.find("동적 모드") != std::string::npos || text.find("동적 배경화면") != std::string::npos ||
                        text.find("동적 스킨") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_TH_TH)
    bool is_emoticon = (text.find("โหมดอิโมติคอน") != std::string::npos || text.find("โหมดอีโมจิ") != std::string::npos ||
                        text.find("โหมดอารมณ์") != std::string::npos || text.find("โหมดสีหน้า") != std::string::npos);
    bool is_static = (text.find("โหมดคงที่") != std::string::npos || text.find("วอลเปเปอร์คงที่") != std::string::npos ||
                      text.find("สกินคงที่") != std::string::npos);
    bool is_animated = (text.find("โหมดเคลื่อนไหว") != std::string::npos || text.find("วอลเปเปอร์เคลื่อนไหว") != std::string::npos ||
                        text.find("สกินเคลื่อนไหว") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_VI_VN)
    bool is_emoticon = (text.find("chế độ biểu tượng cảm xúc") != std::string::npos || text.find("chế độ emoji") != std::string::npos ||
                        text.find("chế độ cảm xúc") != std::string::npos || text.find("chế độ biểu cảm") != std::string::npos);
    bool is_static = (text.find("chế độ tĩnh") != std::string::npos || text.find("hình nền tĩnh") != std::string::npos ||
                      text.find("giao diện tĩnh") != std::string::npos);
    bool is_animated = (text.find("chế độ động") != std::string::npos || text.find("hình nền động") != std::string::npos ||
                        text.find("giao diện động") != std::string::npos);
#else
    // 默认简体中文
    bool is_emoticon = (text.find("表情包模式") != std::string::npos || text.find("表情模式") != std::string::npos ||
                        text.find("情绪模式") != std::string::npos || text.find("切换到表情包") != std::string::npos ||
                        text.find("emoji模式") != std::string::npos);
    bool is_static = (text.find("静态模式") != std::string::npos || text.find("静态壁纸") != std::string::npos ||
                      text.find("静态皮肤") != std::string::npos);
    bool is_animated = (text.find("动态模式") != std::string::npos || text.find("动态壁纸") != std::string::npos ||
                        text.find("动态皮肤") != std::string::npos);
#endif
    
    if (is_emoticon) {
        result.action = "SetEmoticonMode";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到表情包模式请求");
        return;
    }
    
    if (is_static) {
        result.action = "SetStaticMode";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到静态模式请求");
        return;
    }
    
    if (is_animated) {
        result.action = "SetAnimatedMode";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到动态模式请求");
        return;
    }
    
    // 默认情况：设置为动态模式
    result.action = "SetAnimatedMode";
    result.confidence = 0.7f;
    ESP_LOGI(TAG, "使用默认显示模式: 动态");
}

void LocalIntentDetector::ExtractSubtitleParameters(const std::string& text, IntentResult& result) {
    // 检查打开/关闭字幕关键词
#ifdef CONFIG_LANGUAGE_ZH_CN
    bool is_show = (text.find("打开") != std::string::npos || text.find("开启") != std::string::npos ||
                    text.find("显示") != std::string::npos);
    bool is_hide = (text.find("关闭") != std::string::npos || text.find("隐藏") != std::string::npos ||
                    text.find("关掉") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_EN_US)
    bool is_show = (text.find("show") != std::string::npos || text.find("enable") != std::string::npos ||
                    text.find("display") != std::string::npos || text.find("on") != std::string::npos);
    bool is_hide = (text.find("hide") != std::string::npos || text.find("disable") != std::string::npos ||
                    text.find("turn off") != std::string::npos || text.find("off") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_JA_JP)
    bool is_show = (text.find("表示") != std::string::npos || text.find("オン") != std::string::npos ||
                    text.find("有効") != std::string::npos);
    bool is_hide = (text.find("非表示") != std::string::npos || text.find("オフ") != std::string::npos ||
                    text.find("無効") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_KO_KR)
    bool is_show = (text.find("표시") != std::string::npos || text.find("켜기") != std::string::npos ||
                    text.find("활성화") != std::string::npos || text.find("온") != std::string::npos);
    bool is_hide = (text.find("숨기기") != std::string::npos || text.find("끄기") != std::string::npos ||
                    text.find("비활성화") != std::string::npos || text.find("오프") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_TH_TH)
    bool is_show = (text.find("แสดง") != std::string::npos || text.find("เปิด") != std::string::npos ||
                    text.find("เปิดใช้งาน") != std::string::npos);
    bool is_hide = (text.find("ซ่อน") != std::string::npos || text.find("ปิด") != std::string::npos ||
                    text.find("ปิดใช้งาน") != std::string::npos);
#elif defined(CONFIG_LANGUAGE_VI_VN)
    bool is_show = (text.find("hiện") != std::string::npos || text.find("bật") != std::string::npos ||
                    text.find("kích hoạt") != std::string::npos);
    bool is_hide = (text.find("ẩn") != std::string::npos || text.find("tắt") != std::string::npos ||
                    text.find("vô hiệu hóa") != std::string::npos);
#else
    // 默认简体中文
    bool is_show = (text.find("打开") != std::string::npos || text.find("开启") != std::string::npos ||
                    text.find("显示") != std::string::npos);
    bool is_hide = (text.find("关闭") != std::string::npos || text.find("隐藏") != std::string::npos ||
                    text.find("关掉") != std::string::npos);
#endif
    
    if (is_show) {
        result.action = "ShowSubtitle";
        result.parameters["visible"] = "true";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到打开字幕请求");
        return;
    }
    
    if (is_hide) {
        result.action = "HideSubtitle";
        result.parameters["visible"] = "false";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到关闭字幕请求");
        return;
    }
    
    // 默认情况：切换字幕显示状态
    result.action = "ToggleSubtitle";
    result.confidence = 0.7f;
    ESP_LOGI(TAG, "使用默认字幕操作: 切换");
}

} // namespace intent
