/****************************************************************************
 * dm_pinyin.c — Pinyin input method with dictionary
 *
 * Maps pinyin syllables to Chinese characters.
 * Supports prefix matching, multi-syllable composition.
 ****************************************************************************/

#include "dm_pinyin.h"
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Dictionary: pinyin → hanzi string (most common chars)              */
/* ------------------------------------------------------------------ */

typedef struct
{
  const char *py;
  const char *hz;
} py_entry_t;

static const py_entry_t s_dict[] = {
  {"a", "啊阿呵吖嗄腌锕"},
  {"ai", "爱哎唉哀挨矮碍癌艾隘"},
  {"an", "安暗按案岸俺氨胺"},
  {"ang", "昂肮盎"},
  {"ao", "奥澳傲熬凹袄"},
  {"ba", "八把爸吧罢拔巴霸坝芭捌"},
  {"bai", "百白摆败拜柏"},
  {"ban", "半办班般搬板版扮拌伴瓣"},
  {"bang", "帮棒绑磅榜傍谤"},
  {"bao", "报包宝抱暴薄保爆胞饱"},
  {"bei", "被北背杯倍悲备辈碑"},
  {"ben", "本奔笨"},
  {"beng", "泵蹦崩绷"},
  {"bi", "比笔必避鼻壁毕闭臂逼币弊"},
  {"bian", "边变便遍编辨扁鞭"},
  {"biao", "表标彪镖"},
  {"bie", "别憋鳖"},
  {"bin", "宾彬斌滨"},
  {"bing", "并病兵冰丙饼"},
  {"bo", "波播博伯玻薄剥柏勃脖驳"},
  {"bu", "不步布补捕部簿"},
  {"ca", "擦"},
  {"cai", "才菜猜材财裁采彩踩"},
  {"can", "参餐残惨灿"},
  {"cang", "藏仓苍舱沧"},
  {"cao", "草操曹槽"},
  {"ce", "策测册厕侧"},
  {"ceng", "曾层"},
  {"cha", "茶查差插察叉茬"},
  {"chai", "拆柴"},
  {"chan", "产缠阐搀禅蝉馋"},
  {"chang", "常长场厂唱偿昌敞畅"},
  {"chao", "超朝抄潮吵炒巢"},
  {"che", "车彻撤扯"},
  {"chen", "陈沉称晨尘臣衬趁"},
  {"cheng", "成城程称承呈乘诚惩撑"},
  {"chi", "吃迟持池尺赤齿斥翅痴"},
  {"chong", "充冲虫崇重"},
  {"chou", "抽愁仇丑臭筹稠"},
  {"chu", "出处初除楚触储厨础"},
  {"chua", "歘"},
  {"chuai", "揣踹"},
  {"chuan", "穿传船串川喘"},
  {"chuang", "窗创床闯"},
  {"chui", "吹垂锤炊"},
  {"chun", "春纯唇蠢"},
  {"chuo", "戳绰"},
  {"ci", "此次词刺辞磁瓷慈"},
  {"cong", "从丛聪匆葱"},
  {"cou", "凑"},
  {"cu", "促粗醋"},
  {"cuan", "窜篡"},
  {"cui", "催脆翠崔"},
  {"cun", "村存寸"},
  {"cuo", "错措挫搓"},
  {"da", "大打达答搭瘩"},
  {"dai", "大代带待袋戴呆贷逮"},
  {"dan", "但单弹蛋淡担胆旦"},
  {"dang", "当党挡档荡"},
  {"dao", "到道导刀倒岛盗悼稻"},
  {"de", "的得德"},
  {"deng", "等灯登邓"},
  {"di", "地的低底敌帝弟递滴迪"},
  {"dian", "点电店典颠垫殿"},
  {"diao", "掉调吊钓雕"},
  {"die", "跌爹碟叠蝶"},
  {"ding", "定顶订丁盯钉鼎"},
  {"diu", "丢"},
  {"dong", "东动冬懂洞冻栋"},
  {"dou", "都斗豆逗抖兜"},
  {"du", "读度毒独杜堵赌督"},
  {"duan", "断段短端锻"},
  {"dui", "对队堆"},
  {"dun", "顿盾蹲吨敦"},
  {"duo", "多朵躲夺堕"},
  {"e", "额恶鹅俄"},
  {"ei", "诶"},
  {"en", "恩嗯"},
  {"er", "二而耳儿"},
  {"fa", "发法罚乏伐"},
  {"fan", "反饭翻犯烦范泛番凡"},
  {"fang", "方放房防访纺芳仿"},
  {"fei", "非飞费废肥肺"},
  {"fen", "分份纷粉坟奋"},
  {"feng", "风封丰疯峰锋逢缝蜂枫"},
  {"fo", "佛"},
  {"fu", "服副复夫父富付负妇附福幅伏扶腐"},
  {"ga", "嘎尬"},
  {"gai", "该改盖概"},
  {"gan", "干感敢赶甘肝竿"},
  {"gang", "刚钢纲港岗缸杠"},
  {"gao", "高搞告糕稿"},
  {"ge", "个各歌哥格隔革割"},
  {"gei", "给"},
  {"gen", "跟根"},
  {"geng", "更耕"},
  {"gong", "工公共功攻宫恭供"},
  {"gou", "够狗构沟勾购"},
  {"gu", "古故顾骨谷固鼓孤姑"},
  {"gua", "瓜挂刮"},
  {"guai", "怪乖"},
  {"guan", "关管观官馆惯贯灌"},
  {"guang", "光广逛"},
  {"gui", "归贵鬼规桂柜"},
  {"gun", "滚棍"},
  {"guo", "国过果锅裹"},
  {"ha", "哈"},
  {"hai", "还海害孩嗨"},
  {"han", "汉喊含寒汗韩旱"},
  {"hang", "行航杭"},
  {"hao", "好号浩毫豪"},
  {"he", "和合河何贺喝核荷"},
  {"hei", "黑嘿"},
  {"hen", "很恨狠"},
  {"heng", "横哼衡恒"},
  {"hong", "红宏洪轰虹弘鸿"},
  {"hou", "后厚候猴吼"},
  {"hu", "湖虎互护呼忽胡壶糊"},
  {"hua", "花话化画华划滑"},
  {"huai", "坏怀"},
  {"huan", "还换欢环幻患缓"},
  {"huang", "黄皇慌荒晃煌"},
  {"hui", "会回汇灰挥辉慧毁"},
  {"hun", "混婚魂昏"},
  {"huo", "活火或货获伙惑"},
  {"ji", "几机己记集计技击鸡积基极及急既际季继寄激吉挤济辑籍"},
  {"jia", "家加假价甲架佳驾嫁"},
  {"jian", "见间件建简检健减剑尖坚监兼渐"},
  {"jiang", "将讲江奖降酱疆僵"},
  {"jiao", "叫教交角较脚觉浇焦胶"},
  {"jie", "接解结节姐界借介届截街"},
  {"jin", "进近金今仅尽紧禁劲津"},
  {"jing", "经精京惊景竟境静镜净敬警晶"},
  {"jiong", "窘炯"},
  {"jiu", "就九久酒旧救纠"},
  {"ju", "举局句具据剧居距巨拒俱"},
  {"juan", "卷捐圈倦"},
  {"jue", "决绝觉角掘"},
  {"jun", "军均君菌俊"},
  {"ka", "卡咖喀"},
  {"kai", "开凯慨"},
  {"kan", "看砍刊堪"},
  {"kang", "抗康扛"},
  {"kao", "考靠烤"},
  {"ke", "可科课客刻克颗渴壳"},
  {"ken", "肯啃恳"},
  {"keng", "坑"},
  {"kong", "空恐控孔"},
  {"kou", "口扣"},
  {"ku", "苦哭酷库"},
  {"kua", "夸跨垮"},
  {"kuai", "快块筷"},
  {"kuan", "宽款"},
  {"kuang", "况矿狂框"},
  {"kui", "亏愧溃"},
  {"kun", "困昆捆"},
  {"kuo", "扩括阔"},
  {"la", "拉啦辣蜡"},
  {"lai", "来赖"},
  {"lan", "蓝兰拦栏烂懒"},
  {"lang", "浪狼朗郎"},
  {"lao", "老劳牢捞"},
  {"le", "了乐勒"},
  {"lei", "类泪累雷"},
  {"leng", "冷愣棱"},
  {"li", "里力理利立离例历丽礼粒厉"},
  {"lia", "俩"},
  {"lian", "连联练脸链莲恋"},
  {"liang", "两量亮良凉粮辆"},
  {"liao", "了料聊疗辽"},
  {"lie", "列烈猎裂劣"},
  {"lin", "林临邻淋"},
  {"ling", "另令领零灵铃龄"},
  {"liu", "六流留刘柳"},
  {"long", "龙笼隆拢"},
  {"lou", "楼漏露"},
  {"lu", "路录陆鲁炉卢"},
  {"lv", "绿旅律率虑氯"},
  {"luan", "乱卵"},
  {"lun", "论轮伦"},
  {"luo", "落罗逻络螺"},
  {"ma", "吗妈马嘛骂麻码"},
  {"mai", "买卖麦迈埋"},
  {"man", "满慢漫瞒"},
  {"mang", "忙盲芒茫"},
  {"mao", "猫毛冒帽矛貌"},
  {"me", "么"},
  {"mei", "没美每妹梅媒"},
  {"men", "们门闷"},
  {"meng", "梦猛蒙盟"},
  {"mi", "米密迷蜜秘"},
  {"mian", "面免棉眠"},
  {"miao", "秒妙苗描庙"},
  {"mie", "灭蔑"},
  {"min", "民敏"},
  {"ming", "明名命鸣"},
  {"miu", "谬"},
  {"mo", "没模末莫摸磨墨默魔"},
  {"mou", "某谋"},
  {"mu", "木目母牧墓幕慕暮"},
  {"na", "那拿哪纳娜"},
  {"nai", "乃奶耐"},
  {"nan", "南难男"},
  {"nang", "囊"},
  {"nao", "脑闹"},
  {"ne", "呢"},
  {"nei", "内"},
  {"nen", "嫩"},
  {"neng", "能"},
  {"ni", "你呢泥逆拟"},
  {"nian", "年念粘"},
  {"niang", "娘酿"},
  {"niao", "鸟尿"},
  {"nie", "捏"},
  {"nin", "您"},
  {"ning", "宁凝"},
  {"niu", "牛扭纽"},
  {"nong", "农浓弄"},
  {"nu", "努怒奴"},
  {"nv", "女"},
  {"nuan", "暖"},
  {"nuo", "诺挪"},
  {"o", "哦噢"},
  {"ou", "偶欧呕"},
  {"pa", "怕爬趴帕"},
  {"pai", "拍排派牌"},
  {"pan", "盘判盼叛攀"},
  {"pang", "旁胖"},
  {"pao", "跑炮泡抛"},
  {"pei", "配培佩陪赔"},
  {"pen", "盆喷"},
  {"peng", "朋碰棚蓬捧膨"},
  {"pi", "批皮匹屁披僻"},
  {"pian", "片骗偏便篇"},
  {"piao", "票漂飘"},
  {"pie", "撇"},
  {"pin", "品拼贫频聘"},
  {"ping", "平评瓶屏凭乒"},
  {"po", "破坡婆迫颇泼"},
  {"pu", "扑铺朴葡蒲"},
  {"qi", "起其气期七器奇棋旗企齐骑"},
  {"qia", "恰卡"},
  {"qian", "前千钱签浅牵欠迁"},
  {"qiang", "强墙抢枪腔"},
  {"qiao", "桥敲巧悄瞧"},
  {"qie", "切且窃"},
  {"qin", "亲勤琴侵寝"},
  {"qing", "清情青轻请庆晴"},
  {"qiong", "穷琼"},
  {"qiu", "球求秋丘"},
  {"qu", "去取区曲趣驱"},
  {"quan", "全权劝圈拳"},
  {"que", "确却缺雀"},
  {"qun", "群裙"},
  {"ran", "然燃染"},
  {"rang", "让嚷壤"},
  {"rao", "绕扰"},
  {"re", "热惹"},
  {"ren", "人认任忍仁刃"},
  {"reng", "仍扔"},
  {"ri", "日"},
  {"rong", "容融荣熔溶"},
  {"rou", "肉柔"},
  {"ru", "入如乳儒"},
  {"ruan", "软"},
  {"rui", "锐瑞"},
  {"run", "润"},
  {"ruo", "若弱"},
  {"sa", "撒洒"},
  {"sai", "赛塞"},
  {"san", "三散伞"},
  {"sang", "桑嗓"},
  {"sao", "扫嫂"},
  {"se", "色涩"},
  {"sen", "森"},
  {"seng", "僧"},
  {"sha", "沙啥杀傻纱"},
  {"shai", "晒筛"},
  {"shan", "山善闪衫珊"},
  {"shang", "上商伤尚赏"},
  {"shao", "少绍烧稍勺"},
  {"she", "社设射蛇舌舍涉"},
  {"shei", "谁"},
  {"shen", "什身深神审甚肾"},
  {"sheng", "生声省胜圣升"},
  {"shi", "是时十事使世市师石识实史失始式示室势试"},
  {"shou", "手受收首守寿授售"},
  {"shu", "书数术树属输熟述束"},
  {"shua", "刷耍"},
  {"shuai", "帅摔衰"},
  {"shuan", "拴栓"},
  {"shuang", "双爽"},
  {"shui", "水谁睡税"},
  {"shun", "顺瞬"},
  {"shuo", "说硕"},
  {"si", "四死思似丝私司"},
  {"song", "送松宋颂"},
  {"sou", "搜"},
  {"su", "素速诉苏塑"},
  {"suan", "算酸蒜"},
  {"sui", "随岁碎虽"},
  {"sun", "孙损笋"},
  {"suo", "所缩锁"},
  {"ta", "他她它踏塔"},
  {"tai", "太台态抬泰"},
  {"tan", "谈弹探叹坦炭"},
  {"tang", "糖堂躺趟汤"},
  {"tao", "逃套讨桃淘陶"},
  {"te", "特"},
  {"teng", "疼腾"},
  {"ti", "体提题替踢梯"},
  {"tian", "天田添甜填"},
  {"tiao", "条跳调挑"},
  {"tie", "铁贴"},
  {"ting", "听停挺庭厅亭"},
  {"tong", "同通统痛铜桶筒"},
  {"tou", "头投偷透"},
  {"tu", "土图突途涂兔"},
  {"tuan", "团"},
  {"tui", "推退腿"},
  {"tun", "吞屯"},
  {"tuo", "拖脱托妥"},
  {"wa", "哇挖娃瓦袜"},
  {"wai", "外歪"},
  {"wan", "万完晚玩弯碗湾"},
  {"wang", "王网忘望往汪"},
  {"wei", "为位文围未味微卫喂威"},
  {"wen", "问文闻温稳吻"},
  {"weng", "翁"},
  {"wo", "我握窝卧"},
  {"wu", "五无物武务误午屋污"},
  {"xi", "西习细系喜洗戏吸希息"},
  {"xia", "下夏吓虾侠"},
  {"xian", "先现线县显险鲜限仙"},
  {"xiang", "想向象响项香乡相像箱"},
  {"xiao", "小笑校消效销晓"},
  {"xie", "写些谢歇鞋协"},
  {"xin", "心新信辛欣"},
  {"xing", "行性姓型形兴星醒"},
  {"xiong", "雄胸凶兄熊"},
  {"xiu", "修秀休袖绣"},
  {"xu", "需续许续叙绪畜蓄"},
  {"xuan", "选宣旋悬"},
  {"xue", "学雪血穴"},
  {"xun", "寻训讯迅巡"},
  {"ya", "呀压牙鸭雅芽崖"},
  {"yan", "眼烟言严验岩研延沿盐"},
  {"yang", "样养阳洋杨央氧痒"},
  {"yao", "要药邀摇腰窑谣遥"},
  {"ye", "也业夜叶野爷页"},
  {"yi", "一以已亿义意忆益艺议衣易"},
  {"yin", "因引音阴印银饮"},
  {"ying", "应影营英赢迎硬映"},
  {"yong", "用永勇拥泳涌"},
  {"you", "有又由友右油游优犹"},
  {"yu", "与于鱼雨语玉遇预余域育"},
  {"yuan", "元原远园员圆愿源院"},
  {"yue", "月越约乐跃"},
  {"yun", "云运允晕韵"},
  {"za", "杂砸咋"},
  {"zai", "在再载灾栽"},
  {"zan", "咱暂赞"},
  {"zang", "脏葬"},
  {"zao", "早造遭糟灶躁澡"},
  {"ze", "则责择"},
  {"zei", "贼"},
  {"zen", "怎"},
  {"zeng", "增曾赠"},
  {"zha", "扎炸渣闸眨"},
  {"zhai", "窄摘宅债"},
  {"zhan", "站占战展沾粘"},
  {"zhang", "长张章丈掌账障彰"},
  {"zhao", "找照招着赵召"},
  {"zhe", "这着者折遮"},
  {"zhen", "真阵针振镇珍震"},
  {"zheng", "正政整争证征挣"},
  {"zhi", "之只知至治直指制志值"},
  {"zhong", "中种重众终钟忠"},
  {"zhou", "周洲粥轴舟"},
  {"zhu", "主住注猪竹筑术逐驻"},
  {"zhua", "抓"},
  {"zhuai", "拽"},
  {"zhuan", "转专砖赚"},
  {"zhuang", "装状壮撞庄"},
  {"zhui", "追坠"},
  {"zhun", "准"},
  {"zhuo", "捉桌着卓"},
  {"zi", "子自字资紫仔"},
  {"zong", "总综宗纵"},
  {"zou", "走奏"},
  {"zu", "组足族租祖阻"},
  {"zuan", "钻"},
  {"zui", "最嘴醉"},
  {"zun", "尊遵"},
  {"zuo", "做作左坐座昨"},
};

/* Word-level dictionary (pinyin → word) */
typedef struct
{
  const char *py;
  const char *word;
} py_word_t;

static const py_word_t s_words[] = {
  {"nihao", "你好"},
  {"women", "我们"},
  {"tamen", "他们"},
  {"shijie", "世界"},
  {"zhongguo", "中国"},
  {"xuexi", "学习"},
  {"gongzuo", "工作"},
  {"shenghuo", "生活"},
  {"shijian", "时间"},
  {"kaixin", "开心"},
  {"gaoxing", "高兴"},
  {"xiexie", "谢谢"},
  {"pengyou", "朋友"},
  {"dianhua", "电话"},
  {"diannao", "电脑"},
  {"shouji", "手机"},
  {"meitian", "每天"},
  {"jintian", "今天"},
  {"mingtian", "明天"},
  {"zuotian", "昨天"},
  {"xianzai", "现在"},
  {"yihou", "以后"},
  {"yiqian", "以前"},
  {"yiding", "一定"},
  {"keyi", "可以"},
  {"meiyou", "没有"},
  {"buzhidao", "不知道"},
  {"duibuqi", "对不起"},
  {"meiguanxi", "没关系"},
  {"maicai", "买菜"},
  {"chiyao", "吃药"},
  {"heshui", "喝水"},
  {"kaihui", "开会"},
  {"jiaozuoye", "交作业"},
  {"dadiahua", "打电话"},
  {"daolaji", "倒垃圾"},
  {"lianxi", "练习"},
  {"shuijiao", "睡觉"},
  {"yundong", "运动"},
  {"hecha", "喝茶"},
  {"chifan", "吃饭"},
  {"xizao", "洗澡"},
  {"shuaya", "刷牙"},
  {"paobu", "跑步"},
  {"kanshu", "看书"},
  {"tingge", "听歌"},
  {"kanwang", "看望"},
  {"huilai", "回来"},
  {"chuqu", "出去"},
  {"qilai", "起来"},
  {"xiexie", "谢谢"},
  {"bukeyi", "不可以"},
  {"zaijian", "再见"},
  {"wanshang", "晚上"},
  {"zaochen", "早晨"},
  {"xiawu", "下午"},
  {"shangwu", "上午"},
  {"banye", "半夜"},
  {"zhongwu", "中午"},
  {"jieri", "节日"},
  {"shengri", "生日"},
  {"chunjie", "春节"},
  {"dongtian", "冬天"},
  {"chuntian", "春天"},
  {"xiatian", "夏天"},
  {"qiutian", "秋天"},
};

/* ------------------------------------------------------------------ */
/* Syllable set for segmentation                                      */
/* ------------------------------------------------------------------ */

/* All valid pinyin syllables (sorted by length desc for greedy match) */
static const char *const s_syllables[] = {
  "zhuang","chuang","shuang","zhuai","chuai","shuai",
  "zhang","chang","shang","zheng","cheng","sheng",
  "zhong","chong","zhuan","chuan","shuan",
  "zhui","chui","shui","zhun","chun","shun",
  "zhuo","chuo","shuo","zhua","chua","shua",
  "zhan","chan","shan","zhei","chei","shei",
  "zhen","chen","shen","zhou","chou","shou",
  "zhao","chao","shao","zhei","chei","shei",
  "zhu","chu","shu","zhi","chi","shi",
  "zha","cha","sha",
  "zai","chai","shai","zao","cao","sao",
  "zan","can","san","zen","cen","sen",
  "zei","cou","sou","zui","cui","sui",
  "zuo","cuo","suo","zun","cun","sun",
  "zan","can","san",
  "za","ca","sa","ze","ce","se","zi","ci","si",
  "zeng","ceng","seng",
  "ang","eng","ong",
  "bai","pai","mai","dai","tai","nai","lai","gai","kai","hai",
  "bei","pei","mei","dei","tei","nei","lei","gei","hei",
  "bao","pao","mao","dao","tao","nao","lao","gao","kao","hao",
  "ban","pan","man","dan","tan","nan","lan","gan","kan","han",
  "bang","pang","mang","dang","tang","nang","lang","gang","kang","hang",
  "ben","pen","men","den","nen","gen","ken","hen",
  "beng","peng","meng","deng","teng","neng","leng","geng","keng","heng",
  "bian","pian","mian","dian","tian","nian","lian","jian","qian","xian",
  "biao","piao","miao","diao","tiao","niao","liao","jiao","qiao","xiao",
  "bie","pie","mie","die","tie","nie","lie","jie","qie","xie",
  "bin","pin","min","din","nin","lin","jin","qin","xin",
  "bing","ping","ming","ding","ting","ning","ling","jing","qing","xing",
  "miu","diu","niu","liu","jiu","qiu","xiu",
  "nue","lue","jue","que","xue",
  "bua","pua","mua","dua","tua","nua","lua","gua","kua","hua",
  "buai","puai","muai","guai","kuai","huai",
  "buan","puan","muan","duan","tuan","nuan","luan","guan","kuan","huan",
  "bui","pui","mui","dui","tui","gui","kui","hui",
  "bun","pun","mun","dun","tun","nun","lun","gun","kun","hun",
  "buo","puo","muo","duo","tuo","nuo","luo","guo","kuo","huo",
  "ba","pa","ma","da","ta","na","la","ga","ka","ha",
  "bo","po","mo","de","te","ne","le","ge","ke","he",
  "bi","pi","mi","di","ti","ni","li","ji","qi","xi",
  "bu","pu","mu","du","tu","nu","lu","gu","ku","hu",
  "lv","nv","jv","qv","xv",
  "a","o","e","i","u","v",
  "ai","ei","ao","ou",
  "an","en","ang","eng",
  "er",
};

#define SYL_COUNT (sizeof(s_syllables) / sizeof(s_syllables[0]))

/* Check if string is a valid pinyin syllable */
static int is_syllable(const char *s, int len)
{
  int i;
  for (i = 0; i < (int)SYL_COUNT; i++)
    {
      int sl = (int)strlen(s_syllables[i]);
      if (sl == len && strncmp(s_syllables[i], s, len) == 0)
        return 1;
    }
  return 0;
}

/* Greedy segment pinyin into syllables */
static int segment_pinyin(const char *py, int *offsets, int *lens, int max_segs)
{
  int len = (int)strlen(py);
  int pos = 0;
  int n = 0;
  int l;

  while (pos < len && n < max_segs)
    {
      int found = 0;
      for (l = 6; l >= 1; l--)
        {
          if (pos + l <= len && is_syllable(py + pos, l))
            {
              offsets[n] = pos;
              lens[n] = l;
              n++;
              pos += l;
              found = 1;
              break;
            }
        }
      if (!found)
        break;
    }
  return n;
}

/* ------------------------------------------------------------------ */
/* API implementation                                                 */
/* ------------------------------------------------------------------ */

void dm_pinyin_init(void)
{
  /* Static data, nothing to allocate */
}

void dm_pinyin_deinit(void)
{
  /* Static data, nothing to free */
}

/* Look up single syllable → characters */
static const char *lookup_syl(const char *py, int len)
{
  int i;
  int count = (int)(sizeof(s_dict) / sizeof(s_dict[0]));
  for (i = 0; i < count; i++)
    {
      if ((int)strlen(s_dict[i].py) == len &&
          strncmp(s_dict[i].py, py, len) == 0)
        return s_dict[i].hz;
    }
  return NULL;
}

int dm_pinyin_get_cands(const char *pinyin, py_cand_t *out, int maxn)
{
  int n = 0;
  int len;
  int i, j;
  int offsets[8];
  int lens[8];
  int nseg;
  const char *hz;

  if (!pinyin || !pinyin[0] || !out || maxn <= 0)
    return 0;

  len = (int)strlen(pinyin);

  /* 1. Try exact word match first */
  {
    int wcount = (int)(sizeof(s_words) / sizeof(s_words[0]));
    for (i = 0; i < wcount && n < maxn; i++)
      {
        if (strcmp(s_words[i].py, pinyin) == 0)
          {
            out[n].chars = s_words[i].word;
            out[n].chars_len = (int)strlen(s_words[i].word) / 3; /* UTF-8 3 bytes per CJK */
            out[n].matched = s_words[i].py;
            out[n].matched_len = (int)strlen(s_words[i].py);
            n++;
          }
      }
  }

  /* 2. Try prefix word match */
  if (n < maxn)
    {
      int wcount = (int)(sizeof(s_words) / sizeof(s_words[0]));
      for (i = 0; i < wcount && n < maxn; i++)
        {
          if ((int)strlen(s_words[i].py) > len &&
              strncmp(s_words[i].py, pinyin, len) == 0)
            {
              /* Deduplicate */
              int dup = 0;
              for (j = 0; j < n; j++)
                {
                  if (strcmp(out[j].chars, s_words[i].word) == 0)
                    { dup = 1; break; }
                }
              if (!dup)
                {
                  out[n].chars = s_words[i].word;
                  out[n].chars_len = (int)strlen(s_words[i].word) / 3;
                  out[n].matched = s_words[i].py;
                  out[n].matched_len = (int)strlen(s_words[i].py);
                  n++;
                }
            }
        }
    }

  /* 3. Single syllable exact match */
  if (n < maxn)
    {
      hz = lookup_syl(pinyin, len);
      if (hz)
        {
          int hzlen = (int)strlen(hz) / 3;
          int show = hzlen > 8 ? 8 : hzlen;
          out[n].chars = hz;
          out[n].chars_len = show;
          out[n].matched = pinyin;
          out[n].matched_len = len;
          n++;
        }
    }

  /* 4. Prefix syllable match */
  if (n < maxn)
    {
      int count = (int)(sizeof(s_dict) / sizeof(s_dict[0]));
      for (i = 0; i < count && n < maxn; i++)
        {
          int dlen = (int)strlen(s_dict[i].py);
          if (dlen > len && strncmp(s_dict[i].py, pinyin, len) == 0)
            {
              int dup = 0;
              for (j = 0; j < n; j++)
                {
                  if (strcmp(out[j].chars, s_dict[i].hz) == 0)
                    { dup = 1; break; }
                }
              if (!dup)
                {
                  out[n].chars = s_dict[i].hz;
                  out[n].chars_len = (int)strlen(s_dict[i].hz) / 3;
                  out[n].matched = s_dict[i].py;
                  out[n].matched_len = dlen;
                  n++;
                }
            }
        }
    }

  /* 5. Multi-syllable composition */
  if (n < maxn)
    {
      nseg = segment_pinyin(pinyin, offsets, lens, 8);
      if (nseg >= 2)
        {
          char composed[64];
          int cpos = 0;
          int ok = 1;

          for (i = 0; i < nseg; i++)
            {
              hz = lookup_syl(pinyin + offsets[i], lens[i]);
              if (!hz || !hz[0])
                { ok = 0; break; }
              /* Pick first char of this syllable */
              if (cpos + 3 < (int)sizeof(composed))
                {
                  memcpy(composed + cpos, hz, 3);
                  cpos += 3;
                }
            }

          if (ok && cpos > 0)
            {
              composed[cpos] = 0;
              /* Check if already in list */
              int dup = 0;
              for (j = 0; j < n; j++)
                {
                  if (strcmp(out[j].chars, composed) == 0)
                    { dup = 1; break; }
                }
              if (!dup)
                {
                  static char s_composed_buf[64];
                  memcpy(s_composed_buf, composed, cpos + 1);
                  out[n].chars = s_composed_buf;
                  out[n].chars_len = nseg;
                  out[n].matched = pinyin;
                  out[n].matched_len = len;
                  n++;
                }
            }
        }
    }

  return n;
}

int dm_pinyin_get_display(const char *pinyin, char *out, int out_size)
{
  int len;
  int offsets[8];
  int lens[8];
  int nseg;
  int pos = 0;
  int i;

  if (!pinyin || !out || out_size <= 0)
    return 0;

  len = (int)strlen(pinyin);
  if (len <= 0)
    {
      out[0] = 0;
      return 0;
    }

  nseg = segment_pinyin(pinyin, offsets, lens, 8);
  if (nseg <= 1)
    {
      /* Single or no syllable, just copy */
      int copy = len < out_size - 1 ? len : out_size - 1;
      memcpy(out, pinyin, copy);
      out[copy] = 0;
      return copy;
    }

  /* Insert separators */
  for (i = 0; i < nseg && pos < out_size - 1; i++)
    {
      if (i > 0 && pos < out_size - 1)
        out[pos++] = '\'';
      int j;
      for (j = 0; j < lens[i] && pos < out_size - 1; j++)
        out[pos++] = pinyin[offsets[i] + j];
    }
  out[pos] = 0;
  return pos;
}
