// SoundManager - UCS 分类系统模块
// UCSCategory.cpp - Universal Category System 内置分类数据和查询实现
// Built-in UCS category reference data.

#include "UCSCategory.h"
#include <algorithm>
#include <cctype>
#include <sstream>

// Static Data

static bool s_initialized = false;
static std::vector<UCSEntry> s_allEntries;
static std::vector<UCSCategoryNode> s_categoryTree;
static std::unordered_map<std::string, int> s_catIdIndex;       // catId -> index in s_allEntries
static std::unordered_map<std::string, std::string> s_catShortMap; // catShort -> category name
static std::unordered_map<std::string, std::string> s_categoryZhMap;    // EN -> ZH
static std::unordered_map<std::string, std::string> s_subCategoryZhMap; // EN -> ZH

// Helper: add one UCS entry

static void addEntry(const char* cat, const char* sub, const char* catId, const char* catShort,
                     const char* explanation, const char* synonyms,
                     const char* cat_zh, const char* sub_zh,
                     const char* cat_ja = "", const char* sub_ja = "") {
    UCSEntry e;
    e.category = cat;
    e.subCategory = sub;
    e.catId = catId;
    e.catShort = catShort;
    e.explanation = explanation;
    e.synonyms = synonyms;
    e.category_zh = cat_zh;
    e.subCategory_zh = sub_zh;
    e.category_ja = cat_ja;
    e.subCategory_ja = sub_ja;
    
    s_catIdIndex[e.catId] = (int)s_allEntries.size();
    s_catShortMap[e.catShort] = e.category;
    if (!e.category_zh.empty()) s_categoryZhMap[e.category] = e.category_zh;
    if (!e.subCategory_zh.empty()) s_subCategoryZhMap[e.subCategory] = e.subCategory_zh;
    
    s_allEntries.push_back(std::move(e));
}

// Initialize - Load Built-in UCS Data

void UCSCategoryManager::initialize() {
    if (s_initialized) return;
    s_allEntries.reserve(700);

// AIR
    addEntry("AIR","MISC","AIRMisc","AIR","Air sounds not fitting another category","","气体","杂项","空気","その他");
    addEntry("AIR","BLOW","AIRBlow","AIR","Steady air blows, like from a compressed can","compressed air, depressurise, release, puff","气体","吹","空気","吹く");
    addEntry("AIR","BURST","AIRBrst","AIR","Sharp air releases, pressure releases","air blast, air poof, chuff, spurt","气体","爆发/喷射","空気","バースト、爆発");
    addEntry("AIR","HISS","AIRHiss","AIR","Slow air releases, flat tire, leak","air release, exhaust, expel","气体","嘶嘶声","空気","シュー");
    addEntry("AIR","SUCTION","AIRSuck","AIR","Air sucking in, vacuum","vacuum, suck, suction","气体","抽吸","空気","吸う、吸引");

// AIRCRAFT
    addEntry("AIRCRAFT","MISC","AEROMisc","AERO","Aircraft not fitting another category","blimp, glider, hot air balloon, ultralight","航空器","杂项","航空機、飛行機","その他");
    addEntry("AIRCRAFT","HELICOPTER","AEROHeli","AERO","Helicopters of all types","gyrocopter, whirlybird, chopper","航空器","直升机","航空機、飛行機","ヘリコプター");
    addEntry("AIRCRAFT","JET","AEROJet","AERO","Commercial and private jet aircraft","airliner, jetliner, jumbo jet, 737, 747","航空器","喷气式飞机","航空機、飛行機","ジェット");
    addEntry("AIRCRAFT","MILITARY","AEROMil","AERO","Military aircraft and drones","fighter jet, bomber, stealth, F18, F16","航空器","军用","航空機、飛行機","軍用機");
    addEntry("AIRCRAFT","PROP","AEROProp","AERO","Propeller aircraft","biplane, turboprop, stunt plane, crop duster","航空器","螺旋桨飞机","航空機、飛行機","プロペラ");
    addEntry("AIRCRAFT","ROCKET","AERORckt","AERO","Rockets and rocket engines","rocket, missile, jetpack, space shuttle","航空器","火箭","航空機、飛行機","ロケット");

// ALARMS
    addEntry("ALARMS","MISC","ALRMMisc","ALRM","Alarm sounds","","警报","杂项","アラーム","その他");
    addEntry("ALARMS","BELL","ALRMBell","ALRM","Bell-based alarms","fire bell, school bell, railroad crossing bell","警报","警铃","アラーム","ベル");
    addEntry("ALARMS","BUZZER","ALRMBuzr","ALRM","Buzzer alarms","door buzzer, game show buzzer","警报","蜂鸣器","アラーム","ブザー");
    addEntry("ALARMS","CLOCK","ALRMClok","ALRM","Alarm clocks","digital alarm clock","警报","警钟","アラーム","クロック");
    addEntry("ALARMS","ELECTRONIC","ALRMElec","ALRM","Electronic alarms","fire detector, smoke detector","警报","电器","アラーム","電子");
    addEntry("ALARMS","SIREN","ALRMSirn","ALRM","Mechanical sirens","klaxon, ahooga horn, air raid siren","警报","警报","アラーム","サイレン");

// AMBIENCE
    addEntry("AMBIENCE","MISC","AMBMisc","AMB","Ambience sounds","","环境","杂项","アンビエンス","その他");
    addEntry("AMBIENCE","AMUSEMENT","AMBAmus","AMB","Theme parks and carnivals","disneyland, theme park, carnival, fair","环境","娱乐","アンビエンス","遊園地");
    addEntry("AMBIENCE","BIRDSONG","AMBBird","AMB","Group songbirds","dawn chorus, morning chorus, chirping","环境","鸟鸣","アンビエンス","鳥の鳴き声");
    addEntry("AMBIENCE","CELEBRATION","AMBCele","AMB","Parties, celebrations","public gathering, event, party, gala","环境","庆典","アンビエンス","祝福");
    addEntry("AMBIENCE","CONSTRUCTION","AMBCnst","AMB","Construction scenes","road construction, building, carpentry","环境","施工","アンビエンス","建設、建築");
    addEntry("AMBIENCE","DESERT","AMBDsrt","AMB","Desert ambiences","desert, tumbleweed, oasis, arid","环境","沙漠","アンビエンス","砂漠");
    addEntry("AMBIENCE","DESIGNED","AMBDsgn","AMB","Artificial/Fantasy ambiences","artificial, fantasy, sci fi, pandora","环境","设计后的","アンビエンス","デザイン");
    addEntry("AMBIENCE","EMERGENCY","AMBEmrg","AMB","Emergency scenes","crime scene, fire fighting, police","环境","紧急状态","アンビエンス","エネルギー");
    addEntry("AMBIENCE","FARM","AMBFarm","AMB","Farm ambiences","farm, harvest, ranch, barn, pasture","环境","农场","アンビエンス","農場、牧場");
    addEntry("AMBIENCE","FOREST","AMBForst","AMB","Forest recordings","forest, woods, rainforest","环境","森林","アンビエンス","森、森林");
    addEntry("AMBIENCE","INDUSTRIAL","AMBInd","AMB","Industrial warehouses/factories","factory, manufacturing, plant, warehouse","环境","工业","アンビエンス","産業");
    addEntry("AMBIENCE","MARKET","AMBMrkt","AMB","Market activity and crowds","market, bazaar, flea market, vendor","环境","市场","アンビエンス","マーケット");
    addEntry("AMBIENCE","NAUTICAL","AMBNaut","AMB","Ships at sea, shipyards","nautical, shipyard, seafaring, marine","环境","航海","アンビエンス","自然");
    addEntry("AMBIENCE","OFFICE","AMBOffc","AMB","Workplaces, offices","office, cubicle, workplace","环境","办公室","アンビエンス","オフィス");
    addEntry("AMBIENCE","PUBLIC PLACE","AMBPubl","AMB","Stores, lobbies, museums","store, lobby, mall, museum, gallery","环境","公共场所","アンビエンス","公共の場");
    addEntry("AMBIENCE","RESTAURANT & BAR","AMBRest","AMB","Restaurants and bars","restaurant, bar, pub, cafe, diner","环境","餐馆","アンビエンス","レストラン");
    addEntry("AMBIENCE","ROOM TONE","AMBRoom","AMB","Simple room tones","room tone, dead air, quiet air","环境","房间底噪","アンビエンス","室内音");
    addEntry("AMBIENCE","RURAL","AMBRurl","AMB","Rural and countryside","countryside, prairie, mountain, meadow","环境","农村","アンビエンス","農村");
    addEntry("AMBIENCE","SCHOOL","AMBSchl","AMB","Schools","grade school, highschool, college, classroom","环境","校园","アンビエンス","学校");
    addEntry("AMBIENCE","SEASIDE","AMBSea","AMB","Beach and oceanside","beach, seaside, shore, pier, harbour","环境","海边","アンビエンス","シーサイド");
    addEntry("AMBIENCE","SPORT","AMBSprt","AMB","Sporting events","football, basketball, baseball, tennis","环境","运动场","アンビエンス","スポーツ");
    addEntry("AMBIENCE","SUBURBAN","AMBSubn","AMB","Suburban streets","suburb, residential, neighborhood","环境","郊区","アンビエンス","郊外");
    addEntry("AMBIENCE","SWAMP","AMBSwmp","AMB","Swamps and marshes","frog, bog, swamp, marsh, everglades","环境","沼泽地","アンビエンス","沼地");
    addEntry("AMBIENCE","TRAFFIC","AMBTraf","AMB","Busy traffic","freeway, highway, motorway","环境","交通","アンビエンス","交通");
    addEntry("AMBIENCE","URBAN","AMBUrbn","AMB","City ambiences","city, downtown, metropolitan, urban","环境","城市","アンビエンス","都市");
    addEntry("AMBIENCE","WEATHER","AMBWthr","AMB","Weather ambiences","rain, wind, storm, thunder","环境","天气","アンビエンス","天気");

// ANIMALS
    addEntry("ANIMALS","MISC","ANMLMisc","ANML","Animal sounds","","动物","杂项","動物","その他");
    addEntry("ANIMALS","BEAR","ANMLBear","ANML","Bears","bear, grizzly, polar bear","动物","熊","動物","クマ");
    addEntry("ANIMALS","BIRD","ANMLBird","ANML","Birds","bird, songbird, raptor, parrot","动物","鸟类","動物","鳥");
    addEntry("ANIMALS","CAT","ANMLCat","ANML","Cats","cat, kitten, feline, meow, purr","动物","猫","動物","猫");
    addEntry("ANIMALS","DOG","ANMLDog","ANML","Dogs","dog, puppy, bark, growl, whine","动物","狗","動物","犬");
    addEntry("ANIMALS","HORSE","ANMLHrse","ANML","Horses","horse, pony, stallion, neigh, gallop","动物","马","動物","馬");
    addEntry("ANIMALS","INSECT","ANMLInst","ANML","Insects","insect, bee, fly, cricket, mosquito","动物","昆虫","動物","昆虫");

// BELLS
    addEntry("BELLS","MISC","BELLMisc","BELL","Bell sounds","","钟","杂项","ベル","その他");
    addEntry("BELLS","CHURCH","BELLChrc","BELL","Church bells","church bell, chapel bell, cathedral","钟","教堂钟","ベル","教会");
    addEntry("BELLS","SMALL","BELLSmll","BELL","Small bells","sleigh bell, jingle bell, hand bell","钟","小钟","ベル","小");

// CARTOON
    addEntry("CARTOON","MISC","CRTNMisc","CRTN","Cartoon sounds","","卡通","杂项","カートゥーン","その他");
    addEntry("CARTOON","BOING","CRTNBong","CRTN","Boing/spring sounds","boing, spring, bounce","卡通","弹跳","カートゥーン","ボヨン");
    addEntry("CARTOON","SPLAT","CRTNSplt","CRTN","Splat/squash sounds","splat, squash, squish","卡通","飞溅","カートゥーン","スプラット");

// CROWDS
    addEntry("CROWDS","MISC","CRWDMisc","CRWD","Crowd sounds","","人群","杂项","群衆","その他");
    addEntry("CROWDS","LARGE","CRWDLrge","CRWD","Large crowds","large crowd, stadium, arena","人群","大型","群衆","大");
    addEntry("CROWDS","MEDIUM","CRWDMed","CRWD","Medium crowds","medium crowd, gathering","人群","中型","群衆","中");
    addEntry("CROWDS","SMALL","CRWDSmll","CRWD","Small crowds","small crowd, group","人群","小型","群衆","小");
    addEntry("CROWDS","SPORT","CRWDSprt","CRWD","Sports crowds","sports crowd, cheer, applause","人群","运动","群衆","スポーツ");

// DOMESTIC
    addEntry("DOMESTIC","MISC","DOMMisc","DOM","Domestic sounds","","家庭","杂项","家庭","その他");
    addEntry("DOMESTIC","BATHROOM","DOMBath","DOM","Bathroom sounds","toilet, shower, bath, faucet","家庭","浴室","家庭","バスルーム");
    addEntry("DOMESTIC","KITCHEN","DOMKtcn","DOM","Kitchen sounds","cooking, pots, pans, dishes","家庭","厨房","家庭","キッチン");

// DOORS
    addEntry("DOORS","MISC","DRMisc","DR","Door sounds","","门","杂项","ドア","その他");
    addEntry("DOORS","CLOSE","DRCls","DR","Door close","door close, slam, shut","门","关","ドア","閉まる");
    addEntry("DOORS","OPEN","DROpn","DR","Door open","door open, creak","门","开","ドア","開く");
    addEntry("DOORS","KNOCK","DRKnck","DR","Door knock","knock, rap, tap","门","敲","ドア","ノック");

// ELECTRONICS
    addEntry("ELECTRONICS","MISC","ELECMisc","ELEC","Electronic sounds","","电子","杂项","電子","その他");
    addEntry("ELECTRONICS","BEEP","ELECBeep","ELEC","Beeps and tones","beep, tone, bleep","电子","哔","電子","ビープ");
    addEntry("ELECTRONICS","COMPUTER","ELECComp","ELEC","Computer sounds","keyboard, mouse click, hard drive","电子","电脑","電子","コンピュータ");
    addEntry("ELECTRONICS","PHONE","ELECPhn","ELEC","Phone sounds","ring, notification, vibrate","电子","电话","電子","電話");

// EXPLOSIONS
    addEntry("EXPLOSIONS","MISC","EXPLMisc","EXPL","Explosion sounds","","爆炸","杂项","爆発","その他");
    addEntry("EXPLOSIONS","BOMB","EXPLBomb","EXPL","Bomb explosions","bomb, blast, detonation","爆炸","炸弹","爆発","爆弾");
    addEntry("EXPLOSIONS","DESIGNED","EXPLDsgn","EXPL","Designed explosions","cinematic explosion, movie explosion","爆炸","设计的","爆発","デザイン");

// FIRE
    addEntry("FIRE","MISC","FIREMisc","FIRE","Fire sounds","","火","杂项","火災","その他");
    addEntry("FIRE","CAMPFIRE","FIRECamp","FIRE","Campfire","campfire, bonfire, crackling","火","篝火","火災","焚き火");
    addEntry("FIRE","LARGE","FIRELrge","FIRE","Large fire","large fire, inferno, blaze","火","大火","火災","大火");

// FOLEY
    addEntry("FOLEY","MISC","FOLYMisc","FOLY","Foley sounds","","拟音","杂项","フォーリー","その他");
    addEntry("FOLEY","BODY","FOLYBody","FOLY","Body foley","body fall, body hit, punch","拟音","身体","フォーリー","体");
    addEntry("FOLEY","CLOTH","FOLYClth","FOLY","Clothing foley","cloth, fabric, rustling, swish","拟音","服装","フォーリー","衣服");
    addEntry("FOLEY","FOOTSTEP","FOLYFoot","FOLY","Footstep foley","footstep, walk, run, shoe","拟音","脚步","フォーリー","足音");
    addEntry("FOLEY","MOVEMENT","FOLYMvmt","FOLY","Movement foley","movement, handling, interaction","拟音","动作","フォーリー","動き");

// GLASS
    addEntry("GLASS","MISC","GLSSMisc","GLSS","Glass sounds","","玻璃","杂项","ガラス","その他");
    addEntry("GLASS","BREAK","GLSSBrk","GLSS","Glass break","glass break, shatter, smash","玻璃","碎裂","ガラス","割れ");
    addEntry("GLASS","CLINK","GLSSClnk","GLSS","Glass clink","clink, toast, tap","玻璃","碰杯","ガラス","チリン");

// HORROR
    addEntry("HORROR","MISC","HRRMisc","HRR","Horror sounds","","恐怖","杂项","ホラー","その他");
    addEntry("HORROR","CREATURE","HRRCrtr","HRR","Creature sounds","monster, creature, demon, ghost","恐怖","怪物","ホラー","クリーチャー");
    addEntry("HORROR","DESIGNED","HRRDsgn","HRR","Designed horror","scare, tension, dread","恐怖","设计的","ホラー","デザイン");

// HOUSEHOLD
    addEntry("HOUSEHOLD","MISC","HUSHMisc","HUSH","Household sounds","","家居","杂项","家庭用品","その他");
    addEntry("HOUSEHOLD","APPLIANCE","HUSHAppl","HUSH","Appliance sounds","washer, dryer, blender, vacuum","家居","电器","家庭用品","家電");

// HUMAN
    addEntry("HUMAN","MISC","HMNMisc","HMN","Human sounds","","人声","杂项","人間","その他");
    addEntry("HUMAN","BREATH","HMNBrth","HMN","Breathing","breath, inhale, exhale, gasp","人声","呼吸","人間","呼吸");
    addEntry("HUMAN","COUGH","HMNCgh","HMN","Coughing/sneezing","cough, sneeze, throat clear","人声","咳嗽","人間","咳");
    addEntry("HUMAN","CROWD REACTION","HMNCrwd","HMN","Crowd reactions","cheer, applause, boo, laughter","人声","群体反应","人間","群衆反応");
    addEntry("HUMAN","CRY","HMNCry","HMN","Crying","cry, sob, weep, whimper","人声","哭泣","人間","泣く");
    addEntry("HUMAN","EATING","HMNEat","HMN","Eating/drinking","eat, chew, slurp, swallow, drink","人声","进食","人間","食べる");
    addEntry("HUMAN","EFFORT","HMNEfrt","HMN","Physical effort","grunt, strain, lift, push","人声","体力动作","人間","努力");
    addEntry("HUMAN","LAUGH","HMNLaff","HMN","Laughter","laugh, giggle, chuckle, snicker","人声","笑","人間","笑い");
    addEntry("HUMAN","SCREAM","HMNScrm","HMN","Screaming","scream, yell, shriek, shout","人声","尖叫","人間","叫び");
    addEntry("HUMAN","WHISTLE","HMNWhst","HMN","Whistling","whistle, wolf whistle","人声","口哨","人間","口笛");

// IMPACTS
    addEntry("IMPACTS","MISC","IMPTMisc","IMPT","Impact sounds","","撞击","杂项","衝撃","その他");
    addEntry("IMPACTS","BODY","IMPTBody","IMPT","Body impacts","punch, kick, slap, hit","撞击","身体","衝撃","体");
    addEntry("IMPACTS","CRASH","IMPTCrsh","IMPT","Crashes","crash, collision, wreck","撞击","碰撞","衝撃","衝突");
    addEntry("IMPACTS","DESIGNED","IMPTDsgn","IMPT","Designed impacts","cinematic hit, boom, thud","撞击","设计的","衝撃","デザイン");
    addEntry("IMPACTS","METAL","IMPTMetl","IMPT","Metal impacts","clang, clank, gong","撞击","金属","衝撃","金属");
    addEntry("IMPACTS","WOOD","IMPTWood","IMPT","Wood impacts","thud, knock, crack","撞击","木材","衝撃","木");

// INDUSTRIAL
    addEntry("INDUSTRIAL","MISC","INDSMisc","INDS","Industrial sounds","","工业","杂项","産業","その他");
    addEntry("INDUSTRIAL","FACTORY","INDSFact","INDS","Factory sounds","factory, assembly line, machine","工业","工厂","産業","工場");
    addEntry("INDUSTRIAL","TOOLS","INDSTool","INDS","Tool sounds","power tool, drill, saw, grinder","工业","工具","産業","ツール");

// MACHINES
    addEntry("MACHINES","MISC","MCHNMisc","MCHN","Machine sounds","","机械","杂项","マシン","その他");
    addEntry("MACHINES","ENGINE","MCHNEngn","MCHN","Engines","engine, motor, generator","机械","引擎","マシン","エンジン");
    addEntry("MACHINES","HYDRAULIC","MCHNHydr","MCHN","Hydraulic sounds","hydraulic, pneumatic, piston","机械","液压","マシン","油圧");

// METAL
    addEntry("METAL","MISC","MTLMisc","MTL","Metal sounds","","金属","杂项","金属","その他");
    addEntry("METAL","CLANG","MTLClng","MTL","Metal clang","clang, clank, ring","金属","碰撞","金属","チャリン");
    addEntry("METAL","SCRAPE","MTLScrp","MTL","Metal scrape","scrape, grind, scratch","金属","刮擦","金属","擦れ");

// MULTIMEDIA
    addEntry("MULTIMEDIA","MISC","MULTMisc","MULT","UI/multimedia sounds","","多媒体","杂项","マルチメディア","その他");
    addEntry("MULTIMEDIA","GAME","MULTGame","MULT","Game sounds","game, arcade, 8-bit, power-up","多媒体","游戏","マルチメディア","ゲーム");
    addEntry("MULTIMEDIA","NOTIFICATION","MULTNotf","MULT","Notification sounds","notification, alert, ding, ping","多媒体","通知","マルチメディア","通知");
    addEntry("MULTIMEDIA","UI","MULTUI","MULT","UI sounds","click, hover, select, confirm","多媒体","界面","マルチメディア","UI");

// MUSICAL
    addEntry("MUSICAL","MISC","MUSCMisc","MUSC","Musical elements","","音乐","杂项","音楽","その他");
    addEntry("MUSICAL","DRUM","MUSCDrum","MUSC","Drums and percussion","drum, snare, kick, cymbal, hi-hat","音乐","鼓","音楽","ドラム");
    addEntry("MUSICAL","GUITAR","MUSCGtr","MUSC","Guitar sounds","guitar, strum, pick, chord","音乐","吉他","音楽","ギター");
    addEntry("MUSICAL","PIANO","MUSCPno","MUSC","Piano sounds","piano, key, chord, arpeggio","音乐","钢琴","音楽","ピアノ");
    addEntry("MUSICAL","STINGER","MUSCStng","MUSC","Musical stingers","stinger, hit, accent, sting","音乐","尾奏","音楽","スティンガー");

// NATURE
    addEntry("NATURE","MISC","NATRMisc","NATR","Nature sounds","","自然","杂项","自然","その他");
    addEntry("NATURE","EARTHQUAKE","NATREarq","NATR","Earthquake","earthquake, tremor, rumble","自然","地震","自然","地震");
    addEntry("NATURE","RAIN","NATRRain","NATR","Rain","rain, drizzle, downpour, shower","自然","雨","自然","雨");
    addEntry("NATURE","THUNDER","NATRThnd","NATR","Thunder","thunder, lightning, thunderstorm","自然","雷","自然","雷");
    addEntry("NATURE","WATER","NATRWatr","NATR","Water sounds","water, stream, river, creek, splash","自然","水","自然","水");
    addEntry("NATURE","WIND","NATRWind","NATR","Wind","wind, gust, breeze, howling wind","自然","风","自然","風");

// OFFICE
    addEntry("OFFICE","MISC","OFCMisc","OFC","Office sounds","","办公","杂项","オフィス","その他");
    addEntry("OFFICE","PAPER","OFCPapr","OFC","Paper sounds","paper, page turn, crumple, tear","办公","纸张","オフィス","紙");
    addEntry("OFFICE","TYPING","OFCType","OFC","Typing sounds","keyboard, typing, key press","办公","打字","オフィス","タイピング");

// SCI-FI
    addEntry("SCI-FI","MISC","SCFIMisc","SCFI","Sci-fi sounds","","科幻","杂项","SF","その他");
    addEntry("SCI-FI","ALIEN","SCFIAlin","SCFI","Alien sounds","alien, extraterrestrial, ufo","科幻","外星","SF","エイリアン");
    addEntry("SCI-FI","LASER","SCFILasr","SCFI","Laser sounds","laser, blaster, ray gun, phaser","科幻","激光","SF","レーザー");
    addEntry("SCI-FI","ROBOT","SCFIRobt","SCFI","Robot sounds","robot, mech, servo, android","科幻","机器人","SF","ロボット");
    addEntry("SCI-FI","SPACESHIP","SCFIShip","SCFI","Spaceship sounds","spaceship, starship, warp, engine","科幻","宇宙飞船","SF","宇宙船");

// SPORTS
    addEntry("SPORTS","MISC","SPRTMisc","SPRT","Sports sounds","","体育","杂项","スポーツ","その他");
    addEntry("SPORTS","BASEBALL","SPRTBase","SPRT","Baseball","bat, ball, glove, pitch, homerun","体育","棒球","スポーツ","野球");
    addEntry("SPORTS","BASKETBALL","SPRTBskt","SPRT","Basketball","dribble, bounce, swish, net","体育","篮球","スポーツ","バスケ");
    addEntry("SPORTS","BOXING","SPRTBox","SPRT","Boxing/martial arts","punch, bell, ring, fight","体育","拳击","スポーツ","ボクシング");

// TECHNOLOGY
    addEntry("TECHNOLOGY","MISC","TECHMisc","TECH","Technology sounds","","科技","杂项","テクノロジー","その他");
    addEntry("TECHNOLOGY","CAMERA","TECHCam","TECH","Camera sounds","shutter, click, film advance","科技","相机","テクノロジー","カメラ");
    addEntry("TECHNOLOGY","PRINTER","TECHPrnt","TECH","Printer sounds","printer, copier, fax","科技","打印机","テクノロジー","プリンター");

// TELEVISION
    addEntry("TELEVISION","MISC","TVMisc","TV","Television sounds","","电视","杂项","テレビ","その他");
    addEntry("TELEVISION","STATIC","TVStat","TV","Static/noise","static, white noise, fuzz, hiss","电视","静态噪音","テレビ","静的");

// TOOLS
    addEntry("TOOLS","MISC","TOOLMisc","TOOL","Tool sounds","","工具","杂项","ツール","その他");
    addEntry("TOOLS","HAND","TOOLHand","TOOL","Hand tools","hammer, screwdriver, wrench, pliers","工具","手动","ツール","手");
    addEntry("TOOLS","POWER","TOOLPowr","TOOL","Power tools","drill, saw, grinder, sander","工具","电动","ツール","電動");

// TRAINS
    addEntry("TRAINS","MISC","TRNMisc","TRN","Train sounds","","火车","杂项","列車","その他");
    addEntry("TRAINS","HORN","TRNHorn","TRN","Train horn/whistle","train horn, whistle, air horn","火车","汽笛","列車","汽笛");
    addEntry("TRAINS","PASS BY","TRNPass","TRN","Train pass by","train pass, rail, tracks","火车","经过","列車","通過");
    addEntry("TRAINS","STEAM","TRNSteam","TRN","Steam train","steam engine, locomotive, chug","火车","蒸汽","列車","蒸気");
    addEntry("TRAINS","SUBWAY","TRNSub","TRN","Subway/metro","subway, metro, underground","火车","地铁","列車","地下鉄");

// VEHICLES
    addEntry("VEHICLES","MISC","VEHMisc","VEH","Vehicle sounds","","交通工具","杂项","乗り物","その他");
    addEntry("VEHICLES","CAR","VEHCar","VEH","Car sounds","car, sedan, coupe, hatchback","交通工具","轿车","乗り物","車");
    addEntry("VEHICLES","HORN","VEHHorn","VEH","Vehicle horn","horn, honk, beep","交通工具","喇叭","乗り物","ホーン");
    addEntry("VEHICLES","MOTORCYCLE","VEHMoto","VEH","Motorcycle sounds","motorcycle, bike, harley, scooter","交通工具","摩托车","乗り物","オートバイ");
    addEntry("VEHICLES","SIREN","VEHSirn","VEH","Vehicle sirens","police siren, ambulance, fire truck","交通工具","警笛","乗り物","サイレン");
    addEntry("VEHICLES","TRUCK","VEHTrck","VEH","Truck sounds","truck, semi, lorry, big rig","交通工具","卡车","乗り物","トラック");

// WARFARE
    addEntry("WARFARE","MISC","WARMisc","WAR","Warfare sounds","","战争","杂项","戦争","その他");
    addEntry("WARFARE","CANNON","WARCann","WAR","Cannon fire","cannon, artillery, mortar","战争","大炮","戦争","大砲");
    addEntry("WARFARE","DESIGNED","WARDsgn","WAR","Designed warfare","cinematic warfare, battle","战争","设计的","戦争","デザイン");
    addEntry("WARFARE","GUN","WARGun","WAR","Gunshots","gun, rifle, pistol, shotgun, machine gun","战争","枪","戦争","銃");
    addEntry("WARFARE","RELOAD","WARRlod","WAR","Gun reload/mechanism","reload, cock, chamber, magazine","战争","换弹","戦争","リロード");

// WATER
    addEntry("WATER","MISC","WTRMisc","WTR","Water sounds","","水","杂项","水","その他");
    addEntry("WATER","DRIP","WTRDrip","WTR","Water drip","drip, drop, plop","水","滴水","水","滴り");
    addEntry("WATER","OCEAN","WTROcn","WTR","Ocean/waves","ocean, wave, surf, tide, breaker","水","海洋","水","海洋");
    addEntry("WATER","POUR","WTRPour","WTR","Water pour","pour, fill, stream, flow","水","倒水","水","注ぐ");
    addEntry("WATER","SPLASH","WTRSplsh","WTR","Water splash","splash, splatter, slosh","水","溅水","水","スプラッシュ");
    addEntry("WATER","UNDERWATER","WTRUndr","WTR","Underwater sounds","underwater, submerged, bubbles","水","水下","水","水中");

// WEATHER
    addEntry("WEATHER","MISC","WTHRMisc","WTHR","Weather sounds","","天气","杂项","天気","その他");
    addEntry("WEATHER","HAIL","WTHRHail","WTHR","Hail","hail, ice pellets, sleet","天气","冰雹","天気","あられ");
    addEntry("WEATHER","RAIN","WTHRRain","WTHR","Rain sounds","rain, drizzle, downpour","天气","雨","天気","雨");
    addEntry("WEATHER","SNOW","WTHRSnow","WTHR","Snow sounds","snow, blizzard, crunch","天气","雪","天気","雪");
    addEntry("WEATHER","THUNDER","WTHRThnd","WTHR","Thunder and lightning","thunder, lightning bolt, strike","天气","雷","天気","雷");
    addEntry("WEATHER","WIND","WTHRWind","WTHR","Wind sounds","wind, gale, gust, breeze, howl","天气","风","天気","風");

// WHOOSH
    addEntry("WHOOSH","MISC","WHSHMisc","WHSH","Whoosh sounds","","嗖","杂项","スウィッシュ","その他");
    addEntry("WHOOSH","AIR","WHSHAir","WHSH","Air whoosh","air whoosh, fly by, swish","嗖","空气","スウィッシュ","空気");
    addEntry("WHOOSH","DESIGNED","WHSHDsgn","WHSH","Designed whoosh","cinematic whoosh, transition","嗖","设计的","スウィッシュ","デザイン");

// WOOD
    addEntry("WOOD","MISC","WOODMisc","WOOD","Wood sounds","","木","杂项","木材","その他");
    addEntry("WOOD","BREAK","WOODBrk","WOOD","Wood break","wood break, snap, crack, splinter","木","断裂","木材","割れ");
    addEntry("WOOD","CREAK","WOODCrk","WOOD","Wood creak","creak, groan, squeak","木","吱嘎","木材","きしみ");

    // Build the category tree
    buildCategoryTree();
    s_initialized = true;
}

// Build Category Tree

void UCSCategoryManager::buildCategoryTree() {
    std::unordered_map<std::string, int> catIndexMap;
    
    for (auto& entry : s_allEntries) {
        auto it = catIndexMap.find(entry.category);
        if (it == catIndexMap.end()) {
            UCSCategoryNode node;
            node.name = entry.category;
            node.catShort = entry.catShort;
            node.name_zh = entry.category_zh;
            node.entries.push_back(entry);
            node.totalCount = 1;
            catIndexMap[entry.category] = (int)s_categoryTree.size();
            s_categoryTree.push_back(std::move(node));
        } else {
            s_categoryTree[it->second].entries.push_back(entry);
            s_categoryTree[it->second].totalCount++;
        }
    }
    
    // Sort tree by category name
    std::sort(s_categoryTree.begin(), s_categoryTree.end(),
              [](const UCSCategoryNode& a, const UCSCategoryNode& b) {
                  return a.name < b.name;
              });
}

// Query Functions

const std::vector<UCSEntry>& UCSCategoryManager::getAllEntries() {
    return s_allEntries;
}

const std::vector<UCSCategoryNode>& UCSCategoryManager::getCategoryTree() {
    return s_categoryTree;
}

std::string UCSCategoryManager::toLowerStr(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return result;
}

bool UCSCategoryManager::containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    std::string h = toLowerStr(haystack);
    std::string n = toLowerStr(needle);
    return h.find(n) != std::string::npos;
}

std::vector<const UCSEntry*> UCSCategoryManager::search(const std::string& query, int maxResults) {
    std::vector<const UCSEntry*> results;
    if (query.empty()) return results;
    
    std::string q = toLowerStr(query);
    
    for (const auto& entry : s_allEntries) {
        if ((int)results.size() >= maxResults) break;
        
        if (containsIgnoreCase(entry.category, q) ||
            containsIgnoreCase(entry.subCategory, q) ||
            containsIgnoreCase(entry.catId, q) ||
            containsIgnoreCase(entry.explanation, q) ||
            containsIgnoreCase(entry.synonyms, q) ||
            containsIgnoreCase(entry.category_zh, q) ||
            containsIgnoreCase(entry.subCategory_zh, q)) {
            results.push_back(&entry);
        }
    }
    
    return results;
}

std::vector<const UCSEntry*> UCSCategoryManager::getByCategory(const std::string& category) {
    std::vector<const UCSEntry*> results;
    for (const auto& entry : s_allEntries) {
        if (entry.category == category) {
            results.push_back(&entry);
        }
    }
    return results;
}

const UCSEntry* UCSCategoryManager::getByCatId(const std::string& catId) {
    auto it = s_catIdIndex.find(catId);
    if (it != s_catIdIndex.end() && it->second < (int)s_allEntries.size()) {
        return &s_allEntries[it->second];
    }
    return nullptr;
}

std::string UCSCategoryManager::getCategoryByCatShort(const std::string& catShort) {
    auto it = s_catShortMap.find(catShort);
    if (it != s_catShortMap.end()) return it->second;
    return "";
}

std::vector<std::string> UCSCategoryManager::getMainCategories() {
    std::vector<std::string> result;
    for (const auto& node : s_categoryTree) {
        result.push_back(node.name);
    }
    return result;
}

std::vector<const UCSEntry*> UCSCategoryManager::findBySynonym(const std::string& synonym) {
    std::vector<const UCSEntry*> results;
    std::string q = toLowerStr(synonym);
    
    for (const auto& entry : s_allEntries) {
        if (containsIgnoreCase(entry.synonyms, q)) {
            results.push_back(&entry);
        }
    }
    return results;
}

std::vector<const UCSEntry*> UCSCategoryManager::suggestCategory(const std::string& text, int maxResults) {
    std::vector<std::pair<const UCSEntry*, int>> scored;
    std::string q = toLowerStr(text);
    
    // Split text into words
    std::vector<std::string> words;
    std::istringstream iss(q);
    std::string word;
    while (iss >> word) {
        if (word.size() >= 2) words.push_back(word);
    }
    if (words.empty()) return {};
    
    for (const auto& entry : s_allEntries) {
        int score = 0;
        std::string lowerCat = toLowerStr(entry.category);
        std::string lowerSub = toLowerStr(entry.subCategory);
        std::string lowerSyn = toLowerStr(entry.synonyms);
        std::string lowerExp = toLowerStr(entry.explanation);
        
        for (const auto& w : words) {
            if (lowerCat.find(w) != std::string::npos) score += 10;
            if (lowerSub.find(w) != std::string::npos) score += 8;
            if (lowerSyn.find(w) != std::string::npos) score += 5;
            if (lowerExp.find(w) != std::string::npos) score += 3;
            // Chinese match
            if (!entry.category_zh.empty() && entry.category_zh.find(w) != std::string::npos) score += 10;
            if (!entry.subCategory_zh.empty() && entry.subCategory_zh.find(w) != std::string::npos) score += 8;
        }
        
        if (score > 0) {
            scored.push_back({&entry, score});
        }
    }
    
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::vector<const UCSEntry*> results;
    for (int i = 0; i < (int)scored.size() && i < maxResults; i++) {
        results.push_back(scored[i].first);
    }
    return results;
}

std::string UCSCategoryManager::getCategoryZh(const std::string& category) {
    auto it = s_categoryZhMap.find(category);
    if (it != s_categoryZhMap.end()) return it->second;
    return "";
}

std::string UCSCategoryManager::getSubCategoryZh(const std::string& subCategory) {
    auto it = s_subCategoryZhMap.find(subCategory);
    if (it != s_subCategoryZhMap.end()) return it->second;
    return "";
}

bool UCSCategoryManager::isInitialized() {
    return s_initialized;
}

int UCSCategoryManager::getEntryCount() {
    return (int)s_allEntries.size();
}

// 版本管理 (F-014)

static const char* kUCSVersion = "9.1.0";

std::string UCSCategoryManager::getVersion() {
    return kUCSVersion;
}

std::vector<std::string> UCSCategoryManager::checkVersionChanges(const std::string& oldVersion) {
    std::vector<std::string> changes;
    if (oldVersion.empty() || oldVersion == kUCSVersion) return changes;
    // 版本变更日志（新版本添加条目时在此记录）
    if (oldVersion < "9.1.0") {
        changes.push_back("UCS 9.1.0: 新增 TECHNOLOGY 主分类 + 12 个子分类");
        changes.push_back("UCS 9.1.0: AMBIENCE/UNDERWATER 改名为 AMBIENCE/AQUATIC");
    }
    return changes;
}

// 用户自定义分类扩展 (F-014)

static std::vector<UCSEntry> s_customEntries;

void UCSCategoryManager::addCustomEntry(const UCSEntry& entry) {
    // 去重：同 catId 不重复添加
    for (const auto& e : s_customEntries) {
        if (e.catId == entry.catId) return;
    }
    s_customEntries.push_back(entry);
    // 同时添加到全局条目，使 search/suggest 可以匹配到
    s_allEntries.push_back(entry);
    // 重建树
    buildCategoryTree();
}

bool UCSCategoryManager::removeCustomEntry(const std::string& catId) {
    auto it = std::remove_if(s_customEntries.begin(), s_customEntries.end(),
        [&](const UCSEntry& e) { return e.catId == catId; });
    if (it == s_customEntries.end()) return false;
    s_customEntries.erase(it, s_customEntries.end());
    // 也从全局条目移除
    s_allEntries.erase(
        std::remove_if(s_allEntries.begin(), s_allEntries.end(),
            [&](const UCSEntry& e) { return e.catId == catId; }),
        s_allEntries.end());
    buildCategoryTree();
    return true;
}

const std::vector<UCSEntry>& UCSCategoryManager::getCustomEntries() {
    return s_customEntries;
}

// F-028-ext: Feature-based UCS classification
//
// Heuristic mapping from 48-dim audio features to UCS category keywords.
// Rules derived from sonic characteristics of each UCS category group:
//
//  voiced_ratio > 0.55                → HUMAN / VOICE
//  voiced_ratio > 0.35, high F0       → ANIMALS / BIRDS
//  attack_ms < 30, ZCR > 0.25        → IMPACTS / DESTRUCTION
//  duration < 0.8s, high crest        → IMPACTS / WHOOSH
//  centroid < 800 Hz                  → AMBIENCE / RUMBLE
//  centroid > 6000 Hz                 → WHOOSH / ELECTRICITY
//  low ZCR, low centroid, long dur    → AMBIENCE
//  flat spectrum (flatness > 0.4)     → AMBIENCE / NOISE
//
// Returns up to maxResults entries from the full UCS table ranked by rule score.

std::vector<const UCSEntry*> UCSCategoryManager::suggestCategoryFromFeatures(
    const AudioFeatureVector& features, int maxResults)
{
    if (!s_initialized || s_allEntries.empty()) return {};

    // Extract readable feature values
    const float voicedRatio   = features.voicedRatio();
    const float medianF0      = features.medianF0();       // Hz
    const float attackTime    = features.attackTime();     // 0..1 normalised
    const float zcr           = features.zeroCrossingRate();
    const float centroid      = features.spectralCentroid();
    const float flatness      = features.spectralFlatness();
    const float duration      = features.features[AudioFeatureVector::GLOBAL_OFFSET + 2]; // seconds
    const float crestFactor   = features.features[AudioFeatureVector::GLOBAL_OFFSET + 3];

    // Build keyword hint list ordered by priority
    // Each string matches UCS category/subCategory/synonyms via existing search()
    std::vector<std::pair<std::string, float>> hints; // (keyword, score)

    // Voice / Human
    if (voicedRatio > 0.55f) {
        float score = 0.5f + voicedRatio * 0.5f;
        hints.push_back({"voice", score});
        hints.push_back({"human", score * 0.8f});
        if (medianF0 > 80.0f && medianF0 < 400.0f)
            hints.push_back({"speech", score});
    }

    // Animals / Birds — voiced but non-speech F0
    if (voicedRatio > 0.3f && (medianF0 > 400.0f || (medianF0 > 1500.0f))) {
        hints.push_back({"birds", 0.6f});
        hints.push_back({"animals", 0.5f});
    }

    // Impacts / Destruction — very short attack + elevated ZCR
    if (attackTime < 0.15f && zcr > 0.2f) {
        float impactScore = (0.15f - attackTime) / 0.15f * 0.7f + zcr * 0.3f;
        hints.push_back({"impacts", impactScore});
        if (crestFactor > 10.0f)
            hints.push_back({"destruction", impactScore * 0.7f});
    }

    // Whoosh / Movement — short duration + high crest + mid-high centroid
    if (duration < 2.0f && crestFactor > 6.0f && centroid > 1500.0f) {
        hints.push_back({"whoosh", 0.55f});
        hints.push_back({"movement", 0.4f});
    }

    // Electricity / UI — very high centroid, fast transient
    if (centroid > 5000.0f && attackTime < 0.2f) {
        hints.push_back({"electricity", 0.6f});
        hints.push_back({"user interface", 0.4f});
    }

    // Ambience — long duration + low centroid + flat spectrum
    if (duration > 5.0f && centroid < 2000.0f && flatness > 0.15f) {
        float ambScore = std::min(1.0f, duration / 30.0f) * 0.5f + flatness * 0.5f;
        hints.push_back({"ambience", ambScore});
        if (centroid < 800.0f)
            hints.push_back({"rumble", ambScore * 0.7f});
    }

    // Water — mid centroid + high flatness + long duration
    if (flatness > 0.25f && centroid > 800.0f && centroid < 4000.0f && duration > 3.0f) {
        hints.push_back({"water", 0.5f});
    }

    // Fire / Explosions — broadband noise + moderate centroid
    if (flatness > 0.3f && centroid > 1000.0f && centroid < 5000.0f && zcr > 0.15f) {
        hints.push_back({"fire", 0.45f});
        hints.push_back({"explosions", 0.4f});
    }

    // Default fallback — will be handled below if candidates remain empty

    // Collect candidates using existing search(), deduplicate by catId
    std::vector<std::pair<const UCSEntry*, float>> candidates;
    std::unordered_map<std::string, float> seen; // catId -> best score

    for (const auto& [keyword, score] : hints) {
        auto results = search(keyword, 5);
        for (const UCSEntry* e : results) {
            auto it = seen.find(e->catId);
            if (it == seen.end() || it->second < score) {
                seen[e->catId] = score;
                // Remove old entry if exists
                candidates.erase(
                    std::remove_if(candidates.begin(), candidates.end(),
                        [&](const std::pair<const UCSEntry*, float>& p) {
                            return p.first->catId == e->catId; }),
                    candidates.end());
                candidates.push_back({e, score});
            }
        }
    }

    // Sort by descending score
    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Fallback: if no candidates found (e.g. all-zero features), return top entries
    if (candidates.empty() && !s_allEntries.empty()) {
        std::vector<const UCSEntry*> out;
        for (int i = 0; i < maxResults && i < (int)s_allEntries.size(); ++i)
            out.push_back(&s_allEntries[i]);
        return out;
    }

    std::vector<const UCSEntry*> out;
    out.reserve(std::min((int)candidates.size(), maxResults));
    for (int i = 0; i < maxResults && i < (int)candidates.size(); ++i)
        out.push_back(candidates[i].first);
    return out;
}
