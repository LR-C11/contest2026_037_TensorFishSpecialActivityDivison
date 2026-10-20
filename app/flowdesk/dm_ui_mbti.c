/****************************************************************************
 * dm_ui_mbti.c — MBTI 4 banks × 52 items, 4-choice (2×2)
 * Each bank: 4 dichotomies × 13 items (odd → no tie).
 * Start randomly picks one bank per test.
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef CONFIG_DESKMATE_APP

#define MBTI_N 52
#define MBTI_BANK_N 4

typedef struct
{
  const char *zh;
  const char *en;
  const char *zh_opt[4];
  const char *en_opt[4];
  uint8_t dim;      /* 0 EI 1 SN 2 TF 3 JP */
  uint8_t pole[4];  /* 0 = E/S/T/J, 1 = I/N/F/P */
} mbti_item_t;

/* pole layout used widely: [0]=first letter path, [1]=second, mixed by data */

static const mbti_item_t s_banks[MBTI_BANK_N][MBTI_N] = {

/* ================= Bank 0 ================= */
{
  { "聚会结束后你通常？","After a party you?",{"回味很久还想约","第二天想独处","约人下半场","打车回家充电"},{"Still buzzing","Need alone time","Want afterparty","Go home recharge"},0,{0,1,0,1} },
  { "陌生聚会上你？","With strangers you?",{"主动认识人","等别人开口","先找熟人聊","找角落观察"},{"Initiate","Wait","Find familiar","Observe"},0,{0,1,0,1} },
  { "遇到难题时你更想？","When stuck you?",{"找人讨论","自己想清楚","边说边理顺","写下来再定"},{"Talk","Think alone","Talk to sort","Write it"},0,{0,1,0,1} },
  { "电话响起时你？","Phone rings you?",{"乐意接起","希望是消息","看是谁再接","先静音再说"},{"Pick up","Prefer text","Depends","Silent first"},0,{0,1,0,1} },
  { "你的精力主要来自？","Energy comes from?",{"与人相处","独处内省","热闹场合","安静空间"},{"People","Solitude","Crowds","Quiet"},0,{0,1,0,1} },
  { "团队讨论对你？","Team talks feel?",{"像充电","比较耗能","越聊越有劲","结束后想静音"},{"Energizing","Draining","Fueling","Need quiet"},0,{0,1,0,1} },
  { "周末你更想？","Weekend you prefer?",{"聚会活动","宅着休息","约朋友出门","在家看书剧"},{"Hang out","Stay in","Go out","Home"},0,{0,1,0,1} },
  { "思考问题时你？","Thinking you?",{"说出来更清楚","想清楚再说","讨论中成型","脑内先过"},{"Think aloud","Then speak","In talk","Inward"},0,{0,1,0,1} },
  { "朋友堆里你常？","With friends you?",{"发起聚会","响应邀请","张罗热闹","按需出现"},{"Initiate","Respond","Organize","Show up"},0,{0,1,0,1} },
  { "空闲时间你更想？","Free time you?",{"找人玩","自己待着","组局","独处充电"},{"See people","Be alone","Host","Solo"},0,{0,1,0,1} },
  { "会议上你通常？","In meetings you?",{"先发言","听完再补充","想到就说","整理后再说"},{"Speak first","Listen then","Say it now","After sorting"},0,{0,1,0,1} },
  { "认识新朋友让你？","New people make you?",{"兴奋","有点压力","充满好奇","想先观察"},{"Excited","Tense","Curious","Watch first"},0,{0,1,0,1} },
  { "表达想法时你？","Expressing ideas you?",{"很容易说出口","内心戏更多","边聊边展开","先在心里演练"},{"Easy","More inner","Expand in talk","Rehearse"},0,{0,1,0,1} },
  { "你更相信？","You trust more?",{"亲眼所见事实","直觉与可能","具体证据","隐约预感"},{"Facts","Hunches","Evidence","Gut"},1,{0,1,0,1} },
  { "描述事情你注重？","Describing you stress?",{"具体细节","整体含义","步骤过程","象征关联"},{"Details","Meaning","Steps","Symbols"},1,{0,1,0,1} },
  { "学新东西你偏好？","Learning you prefer?",{"清晰步骤说明","概念与联想","操作手册","类比框架"},{"Steps","Concepts","Manuals","Analogies"},1,{0,1,0,1} },
  { "你更关注？","You focus on?",{"现在与实际","未来与变化","眼前可操作","可能性版图"},{"Present","Future","Now work","Possibilities"},1,{0,1,0,1} },
  { "做决定时你看重？","Decisions you weigh?",{"已验证做法","新思路想象","过往经验","没人试过"},{"Proven","Novel","Past wins","Untested"},1,{0,1,0,1} },
  { "拿到说明书你？","With a manual you?",{"按步骤做","先把握大意","逐步对照","先猜逻辑"},{"Follow steps","Gist first","Check each","Guess whole"},1,{0,1,0,1} },
  { "回忆往事你更记得？","You recall more?",{"具体场景细节","感受与启发","时间地点人物","意义感悟"},{"Scenes","Insights","Who/when","Meaning"},1,{0,1,0,1} },
  { "你更喜欢？","You prefer?",{"真实案例","理论模型","故事事实","抽象框架"},{"Cases","Models","Stories","Frames"},1,{0,1,0,1} },
  { "创新时你倾向？","When innovating you?",{"改进现有","从零想概念","优化流程","另起炉灶"},{"Improve","Invent","Refine","Fresh"},1,{0,1,0,1} },
  { "读作品你更爱？","Reading you enjoy?",{"写实质朴","隐喻想象","现实主义","象征寓言"},{"Realistic","Metaphor","Realism","Allegory"},1,{0,1,0,1} },
  { "解决问题时你？","Problem solving you?",{"找已知方法","试新路径","用有效的","探索未知"},{"Known methods","New paths","Proven","Untrodden"},1,{0,1,0,1} },
  { "注意力容易在？","Attention goes to?",{"具体事实","模式关联","点状信息","点之间的线"},{"Facts","Patterns","Dots","Lines"},1,{0,1,0,1} },
  { "做计划你最看重？","In plans you value?",{"可执行细节","方向愿景","落地步骤","大图意义"},{"Details","Vision","Steps","Big picture"},1,{0,1,0,1} },
  { "评估方案你优先？","Evaluating you check?",{"逻辑是否成立","是否照顾人","论证严密","大家感受"},{"Logic","People","Rigour","Feelings"},2,{0,1,0,1} },
  { "朋友诉苦时你？","Friend vents you?",{"分析给建议","先听懂安慰","拆解问题","给情绪价值"},{"Advise","Comfort","Break down","Support"},2,{0,1,0,1} },
  { "你更在意？","You care more?",{"公平原则","和谐感受","对事不对人","关系温度"},{"Fairness","Harmony","Issues","Warmth"},2,{0,1,0,1} },
  { "指出问题时你？","When criticizing you?",{"直接说清","尽量委婉","点出核心","先照顾面子"},{"Direct","Soft","Core issue","Save face"},2,{0,1,0,1} },
  { "好的决定应该？","A good decision is?",{"理性站得住","让人被考虑","经得起推敲","体恤相关人"},{"Sound","Considered","Scrutiny","Care"},2,{0,1,0,1} },
  { "争论中你更看重？","In arguments you value?",{"逻辑一致","关系不破","推理正确","气氛别崩"},{"Logic","Bond","Correctness","Vibe"},2,{0,1,0,1} },
  { "评价表现你？","Reviewing work you?",{"按标准打分","先肯定再建议","对齐指标","关注成长感受"},{"Standards","Praise then","Metrics","Growth"},2,{0,1,0,1} },
  { "选方向时你看？","Choosing a path?",{"前景分析","热情意义","利弊数据","内心愿意"},{"Analysis","Passion","Pros/cons","Heart"},2,{0,1,0,1} },
  { "看电影你欣赏？","Films you admire?",{"剧情逻辑","人物情感","结构严密","情绪共鸣"},{"Plot","Emotion","Structure","Resonance"},2,{0,1,0,1} },
  { "规则不合理时？","Unfair rules you?",{"据理力争","考虑执行感受","指出漏洞","体谅难处"},{"Argue","Consider landing","Point flaws","Empathize"},2,{0,1,0,1} },
  { "帮人时你想给？","Helping you offer?",{"解决方案","情绪支持","可执行建议","陪伴理解"},{"Solutions","Support","Advice","Company"},2,{0,1,0,1} },
  { "你更害怕？","You fear more?",{"不公平","伤害到人","失去原则","让人难受"},{"Injustice","Hurting","Losing principles","Pain"},2,{0,1,0,1} },
  { "团队冲突你先？","Team conflict you?",{"分清对错","安抚再谈","厘清责任","先稳关系"},{"Right/wrong","Sooth then","Duty","Relations"},2,{0,1,0,1} },
  { "旅行前你？","Before travel you?",{"提前定行程","大致灵活走","订好每站","方向对就行"},{"Plan ahead","Flexible","Book stops","Direction enough"},3,{0,1,0,1} },
  { "待办/桌面？","Todo/desk?",{"有条理清单","比较随性","定期整理","能用就行"},{"Lists","Loose","Tidy often","Good enough"},3,{0,1,0,1} },
  { "截止临近你？","Near deadline you?",{"接近完成","冲刺更高效","预留缓冲","最后一刻爆发"},{"Almost done","Final push","Buffer","Burst"},3,{0,1,0,1} },
  { "你更喜欢生活？","Life you prefer?",{"有计划可预期","开放留余地","日程清晰","保持弹性"},{"Planned","Open","Clear schedule","Flexible"},3,{0,1,0,1} },
  { "做完选择后？","After choosing?",{"踏实推进","还想再比较","关掉选项表","仍会观望"},{"Settled","Still compare","Close menu","Still watch"},3,{0,1,0,1} },
  { "收拾房间你？","Tidying you?",{"定时整理","需要再理","每周固定","受不了再说"},{"Schedule","When needed","Weekly","When bothers"},3,{0,1,0,1} },
  { "开会你更希望？","Meetings you prefer?",{"有明确议程","自由讨论","按表推进","想到哪聊"},{"Agenda","Open talk","Stick list","Follow thread"},3,{0,1,0,1} },
  { "做事风格你像？","Work style you?",{"做完再下一件","多线并行","一次专注","几件事轮"},{"One then next","Juggle","Focus one","Rotate"},3,{0,1,0,1} },
  { "购物前你？","Before shopping?",{"列好清单","到店再看","目的明确","逛到心动"},{"List","In-store","Purpose","Browse"},3,{0,1,0,1} },
  { "日程表让你觉得？","A tight schedule feels?",{"安心","有点束缚","有掌控感","像被框住"},{"Reassuring","Restrictive","Control","Boxed"},3,{0,1,0,1} },
  { "项目启动你更想？","Project start you?",{"先定节点","边做边调","排出里程碑","做起来再说"},{"Milestones","Adjust as go","Timeline","Figure out"},3,{0,1,0,1} },
  { "你更讨厌？","You dislike more?",{"突然变更计划","死板计划","计划被打乱","毫无灵活"},{"Sudden changes","Rigid plans","Disrupted","No flexibility"},3,{0,1,0,1} },
  { "结束一天时？","Day's end you prefer?",{"清单都完成","随性也好","打勾很爽","有意外也行"},{"All checked","Loose is fine","Love checks","Surprises ok"},3,{0,1,0,1} },
},

/* ================= Bank 1 ================= */
{
  { "长时间社交后你？","After long social time?",{"还想再聊","急需充电","找人续摊","关门静一静"},{"Keep talking","Recharge","Afterparty","Quiet door"},0,{0,1,0,1} },
  { "新同事入职你？","New colleague you?",{"主动打招呼","等对方先来","拉进午餐群","点头微笑就好"},{"Greet first","Wait","Invite lunch","Nod only"},0,{0,1,0,1} },
  { "卡壳时你更会？","Blocked you tend to?",{"拉人头脑风暴","自己啃透","发消息讨论","列清单独想"},{"Brainstorm","Deep think","Message","List alone"},0,{0,1,0,1} },
  { "群里热闹时你？","Busy group chat you?",{"积极接话","默默围观","偶尔插一句","设置免打扰"},{"Chime in","Lurk","Sometimes","Mute"},0,{0,1,0,1} },
  { "让你放松的是？","What relaxes you?",{"朋友聚餐","一个人散步","开黑打游戏","关灯听歌"},{"Dinner out","Solo walk","Games","Music dark"},0,{0,1,0,1} },
  { "开长会你会？","Long meetings you?",{"越开越投入","越来越累","讨论环节兴奋","盼着结束"},{"More into","Drained","Love discuss","Want end"},0,{0,1,0,1} },
  { "假期第一天你？","First vacation day?",{"约人出门","睡到自然醒","组队旅行","宅家补剧"},{"Meet people","Sleep in","Group trip","Binge home"},0,{0,1,0,1} },
  { "有心事时你？","When troubled you?",{"找人倾诉","先自己消化","边走边说","写日记"},{"Talk it out","Digest first","Walk & talk","Journal"},0,{0,1,0,1} },
  { "团队活动里你？","Team events you?",{"活跃气氛","安静配合","带动游戏","完成任务就好"},{"Light up","Support","Lead games","Just finish"},0,{0,1,0,1} },
  { "独处太久会？","Too much alone time?",{"想找人","很自在","有点闷","刚刚好"},{"Want people","Comfortable","Bit dull","Just right"},0,{0,1,0,1} },
  { "头脑风暴时你？","In brainstorm you?",{"先抛点子","先听再补","越说越多","最后总结"},{"Throw ideas","Listen first","More as talk","Summarize"},0,{0,1,0,1} },
  { "陌生人加好友？","Stranger adds you?",{"很快通过","比较犹豫","看情况","先不加"},{"Quick accept","Hesitate","Depends","Skip"},0,{0,1,0,1} },
  { "一天结束时你？","End of day you?",{"还想有人聊","想安静待着","发消息闲扯","关机休息"},{"Want chat","Want quiet","Text around","Power off"},0,{0,1,0,1} },
  { "做项目你更信？","In projects you trust?",{"数据和案例","灵感和预感","实验结果","大胆假设"},{"Data","Inspiration","Experiments","Bold bets"},1,{0,1,0,1} },
  { "讲故事你偏重？","Storytelling you stress?",{"发生过什么","背后意味","时间线","象征隐喻"},{"What happened","What it means","Timeline","Symbols"},1,{0,1,0,1} },
  { "上课/培训你更吃？","Training you absorb via?",{"演示步骤","框架理论","动手操练","比喻联想"},{"Demo steps","Theory","Hands-on","Metaphors"},1,{0,1,0,1} },
  { "五年后你更常想？","You often imagine?",{"眼下的事","五年后的可能","本周计划","人生版图"},{"Now","5-year maybe","This week","Life map"},1,{0,1,0,1} },
  { "选工具时你？","Choosing tools you?",{"用过且稳","愿意试新款","口碑最好","听起来有趣"},{"Used & stable","Try new","Best rated","Sounds fun"},1,{0,1,0,1} },
  { "拼装家具你？","Assembling furniture?",{"严格按图","先拼大形","对照图检查","凭感觉来"},{"Follow diagram","Big shapes","Check against","Feel it"},1,{0,1,0,1} },
  { "旅行记忆你留？","Travel memory keeps?",{"打卡细节","当时的心情","路线图","文化隐喻"},{"Spots","Mood","Route","Meaning"},1,{0,1,0,1} },
  { "工作汇报你爱用？","Reports you like?",{"表格数据","趋势洞察","清单事实","概念图"},{"Tables","Insights","Lists","Concept maps"},1,{0,1,0,1} },
  { "改进产品时你？","Improving a product?",{"打磨现有","想象新品","修痛点","创造新品类"},{"Polish existing","Imagine new","Fix pains","New category"},1,{0,1,0,1} },
  { "看电影类型？","Film genre you?",{"纪录片","科幻寓言","纪实剧","超现实"},{"Docs","Sci-fi allegory","Realistic drama","Surreal"},1,{0,1,0,1} },
  { "解 bug 时你？","Debugging you?",{"查日志复现","猜想根因","二分定位","重构试试"},{"Logs","Guess root","Bisect","Refactor try"},1,{0,1,0,1} },
  { "开会讨论你抓？","In discussions you catch?",{"谁说了什么点","点背后的结构","数据结论","未说出口的"},{"Who said what","Structure","Numbers","Unsaid"},1,{0,1,0,1} },
  { "目标设定你写？","Goals you write as?",{"可量化动作","方向性愿景","截止清单","图景描述"},{"Measurable actions","Vision","Deadline list","Picture"},1,{0,1,0,1} },
  { "同事争论方案你先问？","Debating plans you ask?",{"逻辑通不通","大家感受如何","证据够不够","有没有更好可能"},{"Logic sound?","Feelings?","Evidence?","Better maybes?"},2,{0,1,0,1} },
  { "朋友失恋你？","Friend heartbroken you?",{"分析问题出在哪","陪着听他说","列利弊","递纸巾就好"},{"Analyze issue","Listen along","Pros/cons","Tissues"},2,{0,1,0,1} },
  { "分配任务你看？","Task assignment by?",{"谁更胜任","谁更需要机会","效率最优","关系平衡"},{"Best fit","Who needs chance","Efficiency","Balance"},2,{0,1,0,1} },
  { "意见不合时你？","Disagreeing you?",{"摆事实讲逻辑","先找共同点","直说分歧","先稳情绪"},{"Facts & logic","Common ground","Name split","Calm first"},2,{0,1,0,1} },
  { "好领导应该是？","A good leader is?",{"决策清晰正确","体谅下属","目标明确","氛围融洽"},{"Clear & right","Caring","Clear goals","Good vibe"},2,{0,1,0,1} },
  { "被批评时你希望？","When criticized you want?",{"指出问题本身","语气温和","数据支撑","先肯定再改"},{"The issue itself","Gentle tone","Data","Praise first"},2,{0,1,0,1} },
  { "选工作你看重？","Job choice you weigh?",{"发展空间分析","团队人情味","薪资数据","意义感"},{"Growth analysis","Team warmth","Pay data","Meaning"},2,{0,1,0,1} },
  { "处理投诉你？","Handling complaints?",{"按流程判定","先安抚情绪","依条款处理","理解对方难处"},{"By process","Sooth first","By rules","Understand"},2,{0,1,0,1} },
  { "看新闻你更气？","News angers you at?",{"逻辑混乱","人受委屈","数据造假","缺乏共情"},{"Messy logic","People hurt","Fake data","No empathy"},2,{0,1,0,1} },
  { "团队有人摸鱼你？","Someone slacks you?",{"指出贡献不均","私下了解原因","按考核处理","怕伤和气"},{"Unequal work","Ask why","By review","Avoid conflict"},2,{0,1,0,1} },
  { "安慰人你常说？","Comforting you say?",{"我们可以这样解决","我理解你难受","数据显示会好","我在这儿"},{"We can fix it","I get it","Data says ok","I'm here"},2,{0,1,0,1} },
  { "原则与关系冲突？","Principles vs people?",{"原则优先","关系优先","尽量兼得","看场合"},{"Principles first","People first","Both","Depends"},2,{0,1,0,1} },
  { "开会效率低你？","Inefficient meetings?",{"指出逻辑跑偏","关心谁被冷落","要数据结论","怕打断别人"},{"Off logic","Who's left out","Want numbers","Afraid interrupt"},2,{0,1,0,1} },
  { "出发旅行你？","Trip packing you?",{"清单打勾装","感觉缺啥带啥","按日程列","轻装随缘"},{"Checklist","Feel what need","By day","Light pack"},3,{0,1,0,1} },
  { "文件管理你？","Files you keep?",{"文件夹清晰","桌面一堆再说","命名规范","搜索就行"},{"Clear folders","Desktop pile","Naming","Search"},3,{0,1,0,1} },
  { "作业/项目你？","Assignments you?",{"提前几天完","截止前赶","分阶段交","灵感来了做"},{"Days early","Cutoff rush","Phased","When inspired"},3,{0,1,0,1} },
  { "周末日程你？","Weekend schedule?",{"排满活动","空着看看","只定一件大事","完全随性"},{"Packed","Open","One big thing","All loose"},3,{0,1,0,1} },
  { "买大件前你？","Big purchase you?",{"研究对比很久","看中就买","列需求清单","逛着决定"},{"Research long","Buy if love","Needs list","Decide browsing"},3,{0,1,0,1} },
  { "房间状态你？","Your room is?",{"定期收拾","乱中有序","每周大扫除","能住就行"},{"Regular tidy","Organized chaos","Weekly deep","Liveable"},3,{0,1,0,1} },
  { "约会安排你？","Planning a date?",{"提前订好位","到了再看","主题定好细节灵活","临时起意"},{"Book ahead","See on arrival","Theme flexible","Spontaneous"},3,{0,1,0,1} },
  { "多任务时你？","Multitasking you?",{"一件做完再下","同时推进","主次切换","看心情"},{"Finish then next","Push many","Switch priority","By mood"},3,{0,1,0,1} },
  { "去超市你？","Grocery you?",{"列清单","逛着买","按食谱","缺什么补什么"},{"List","Browse","By recipe","Restock"},3,{0,1,0,1} },
  { "被突然加会你？","Surprise meeting you?",{"很烦躁","有点兴奋","看是否打乱计划","无所谓"},{"Annoyed","Slightly excited","If disrupts","Whatever"},3,{0,1,0,1} },
  { "学习计划你？","Study plan you?",{"固定时间表","有空就学","按章节推进","考试前突击"},{"Timetable","When free","By chapters","Cram"},3,{0,1,0,1} },
  { "更讨厌哪种？","Dislike more?",{"计划总被打乱","计划定太死","临时改需求","事事要审批"},{"Plans disrupted","Too rigid","Last-minute changes","Too many approvals"},3,{0,1,0,1} },
  { "理想下班后？","Ideal evening is?",{"清单清零安心躺","随性玩到睡","完成一件爱好","计划明天"},{"List done then rest","Play till sleep","One hobby","Plan tomorrow"},3,{0,1,0,1} },
},

/* ================= Bank 2 ================= */
{
  { "KTV/派对你更？","At parties you?",{"唱到嗨还要续摊","中途想溜","带动气氛","安静听歌"},{"Singing on","Want slip out","Hype crowd","Quiet listen"},0,{0,1,0,1} },
  { "电梯里遇同事？","Colleague in elevator?",{"聊起来","微笑点头","问项目进度","看手机"},{"Chat","Smile nod","Ask project","Phone"},0,{0,1,0,1} },
  { "思路卡住你？","Stuck on ideas you?",{"找人碰","散步自己想","开白板讨论","查资料独研"},{"Bounce off people","Walk alone","Whiteboard","Solo research"},0,{0,1,0,1} },
  { "微信99+你会？","99+ messages you?",{"逐条回","先放着","挑重要的回","开勿扰"},{"Reply each","Leave it","Priority reply","DND"},0,{0,1,0,1} },
  { "充电方式你像？","You recharge like?",{"外向电池越用越满","内向电池越用越少","看对象是谁","看心情"},{"Extro battery","Intro battery","Depends who","By mood"},0,{0,1,0,1} },
  { "头脑风暴前你？","Before brainstorm?",{"跃跃欲想","先需要独处预热","看议题","无所谓"},{"Eager","Warm up alone","Depends topic","Whatever"},0,{0,1,0,1} },
  { "出差周末你？","Work trip weekend?",{"当地约人玩","酒店休息","同事聚餐","自己逛"},{"Meet locals","Hotel rest","Team dinner","Solo explore"},0,{0,1,0,1} },
  { "压力大时你？","Under pressure you?",{"说出来好受","自己扛过去","写下来找人","运动独处"},{"Talk helps","Carry alone","Write & talk","Solo exercise"},0,{0,1,0,1} },
  { "团建你更像？","Team building you?",{"组织者","安静参与者","气氛组","任务执行"},{"Organizer","Quiet joiner","Hype crew","Task runner"},0,{0,1,0,1} },
  { "被邀请突然聚会？","Surprise invite you?",{"立刻答应","想找借口不去","看都有谁","看累不累"},{"Yes instantly","Excuse out","Who's going","How tired"},0,{0,1,0,1} },
  { "讨论时你打断吗？","You interrupt in talks?",{"经常边想边说","很少，想好再说","兴奋时会","几乎不会"},{"Often think aloud","Rarely","When excited","Almost never"},0,{0,1,0,1} },
  { "交朋友你？","Making friends you?",{"很快熟络","慢热","看缘分","需要共同事"},{"Quick warm","Slow burn","By fate","Shared work"},0,{0,1,0,1} },
  { "一天里你更有劲在？","You peak when?",{"和人互动后","独处充电后","下午茶社交","深夜自己"},{"After people","After alone","Afternoon social","Late night solo"},0,{0,1,0,1} },
  { "知识来源你信？","Knowledge you trust?",{"教科书与实验","直觉与类比","论文数据","思想实验"},{"Textbooks","Intuition","Papers","Thought exp"},1,{0,1,0,1} },
  { "写文档你先写？","Docs you write first?",{"操作步骤","目标与理念","接口列表","愿景草图"},{"Steps","Purpose","APIs","Vision sketch"},1,{0,1,0,1} },
  { "学乐器你靠？","Learning instruments by?",{"按谱子练","感受旋律","分解练习","即兴试"},{"By score","Feel melody","Drills","Improvise"},1,{0,1,0,1} },
  { "你更容易注意？","You notice more?",{"眼前变化","未来的影子","细节异常","联系网络"},{"Present shifts","Future shadows","Odd details","Connections"},1,{0,1,0,1} },
  { "换手机你？","New phone you?",{"看评测参数","看整体体验口碑","对比表格","看设计概念"},{"Reviews & specs","Overall feel","Compare tables","Design concept"},1,{0,1,0,1} },
  { "菜谱做饭你？","Cooking you?",{"严格量杯","凭手感","按步骤","先想象成品"},{"Measure strictly","By feel","Steps first","Imagine dish"},1,{0,1,0,1} },
  { "读历史你更爱？","History you enjoy?",{"事件与年代","规律与启示","人物细节","隐喻结构"},{"Events & dates","Lessons","People details","Structure"},1,{0,1,0,1} },
  { "产品头脑里是？","Product in your mind is?",{"功能清单","一种体验可能","零件组合","故事意象"},{"Feature list","An experience","Parts","Story image"},1,{0,1,0,1} },
  { "优化流程你先？","Optimizing process?",{"看现有步骤","想理想形态","测数据瓶颈","重新定义问题"},{"Current steps","Ideal form","Data bottlenecks","Redefine"},1,{0,1,0,1} },
  { "欣赏艺术时你？","Art you notice?",{"技法与材质","情绪与象征","构图细节","观者联想"},{"Technique","Emotion/symbol","Composition","Viewer assoc"},1,{0,1,0,1} },
  { "做选择题策略？","Multiple choice strategy?",{"排除确定错的","第一感觉","逐项分析","抓题干关键词"},{"Eliminate","First feel","Analyze each","Keywords"},1,{0,1,0,1} },
  { "听汇报你记？","Listening to reports you keep?",{"结论数字","可能走向","谁负责什么","没说的问题"},{"Numbers","Where it goes","Who owns","Unsaid issues"},1,{0,1,0,1} },
  { "职业发展你画？","Career path you see as?",{"技能台阶","可能性地图","岗位序列","人生叙事"},{"Skill ladder","Possibility map","Job ladder","Life story"},1,{0,1,0,1} },
  { "方案被否你先问？","Proposal rejected you ask?",{"哪里逻辑不对","是不是没人支持","数据哪不够","有没有别的可能"},{"Where logic fails","No support?","Which data","Other maybes?"},2,{0,1,0,1} },
  { "同事哭了你？","Colleague crying you?",{"建议请假/方案","先递纸巾陪着","分析原因","怕说错话"},{"Suggest fixes","Sit with them","Analyze why","Afraid wrong words"},2,{0,1,0,1} },
  { "评优你看？","Awards you judge by?",{"客观指标","谁更被团队需要","业绩数据","人缘贡献"},{"Objective metrics","Who's needed","Numbers","Popularity"},2,{0,1,0,1} },
  { "指出错误你？","Pointing errors you?",{"直接说数据","先关心状态","引用规范","私下委婉"},{"Cite data","Check how they are","By standard","Private soft"},2,{0,1,0,1} },
  { "理想合作是？","Ideal teamwork is?",{"职责清晰逻辑顺","彼此体谅","目标数据一致","氛围互助"},{"Clear roles logic","Mutual care","Shared metrics","Helpful vibe"},2,{0,1,0,1} },
  { "被误解时你？","Misunderstood you?",{"摆证据澄清","很受伤","定义概念","先不解释"},{"Evidence clear","Hurt inside","Define terms","Stay quiet"},2,{0,1,0,1} },
  { "选城市定居看？","City to settle you weigh?",{"发展数据","亲友与温暖","产业机会","文化氛围"},{"Growth data","Friends/warmth","Jobs","Culture"},2,{0,1,0,1} },
  { "客户发火你？","Angry customer you?",{"先弄清事实链","先安抚情绪","按合同条款","表示理解难处"},{"Facts first","Sooth first","By contract","Show empathy"},2,{0,1,0,1} },
  { "你觉得更糟是？","Worse is being?",{"决策不公平","决策伤了人","数据被忽视","大家心寒"},{"Unfair decision","People hurt","Data ignored","Cold hearts"},2,{0,1,0,1} },
  { "团队加班多你？","Team overworks you?",{"流程有问题","大家太辛苦","效率数据","谁在扛活"},{"Broken process","People tired","Efficiency data","Who's carrying"},2,{0,1,0,1} },
  { "道歉重要是？","Apology matters as?",{"承担责任的逻辑","表达真心后悔","给出补救","让对方舒服"},{"Logic of duty","Sincere regret","Make good","Ease them"},2,{0,1,0,1} },
  { "更受不了？","Harder to accept?",{"双标不公","冷漠伤人","规则僵化","数据造假"},{"Double standards","Cold hurt","Rigid rules","Fake data"},2,{0,1,0,1} },
  { "投票/表态你看？","Voting you consider?",{"条款逻辑利弊","对人群影响","数据支持","价值认同"},{"Clause logic","Impact on people","Data","Values"},2,{0,1,0,1} },
  { "衣柜整理你？","Closet you organize?",{"季节分类叠好","穿到哪算哪","按色系排","堆到满"},{"By season","As you go","By color","Pile up"},3,{0,1,0,1} },
  { "邮件处理你？","Email you handle?",{"收件箱清零","攒着慢慢回","定时批处理","看到就回有时忘"},{"Inbox zero","Accumulate","Batch","Reply when see"},3,{0,1,0,1} },
  { "考试复习你？","Exam prep you?",{"计划表推进","考前通宵","按题型刷","看感觉复习"},{"Plan push","All-nighter","By question type","By feel"},3,{0,1,0,1} },
  { "房间装修你？","Home decor you?",{"风格方案先定","住着再调整","功能清单先","灵感来了弄"},{"Style first","Adjust living","Function list","When inspired"},3,{0,1,0,1} },
  { "点外卖你？","Food delivery you?",{"固定几家顺序换","看推荐随便点","按营养搭配","看图选"},{"Fixed rotation","Whatever recommended","By nutrition","By photo"},3,{0,1,0,1} },
  { "待办完成率？","Todo completion?",{"几乎全勾","经常挪到明天","重要项优先完成","写了很多做完很少"},{"Almost all checked","Push to tomorrow","Priority done","Many written few done"},3,{0,1,0,1} },
  { "活动筹备你？","Event planning you?",{"时间线+分工","现场随机应变","关键路径","气氛到了就行"},{"Timeline & roles","Improvise","Critical path","Vibe first"},3,{0,1,0,1} },
  { "同时来三件事？","Three tasks at once?",{"排队一件件来","三头并进","先急后缓","看心情"},{"Queue one by one","Parallel all","Urgent first","By mood"},3,{0,1,0,1} },
  { "读书方式你？","Reading style you?",{"按计划页数","随手翻","做笔记目录","只读感兴趣的"},{"Planned pages","Browse","Notes & TOC","Only interest"},3,{0,1,0,1} },
  { "日程被空出？","Schedule freed you?",{"补上下一件计划","享受空白","整理待办","随便刷手机"},{"Fill next plan","Enjoy blank","Tidy todos","Scroll phone"},3,{0,1,0,1} },
  { "项目截止提前？","Deadline moved earlier?",{"马上改计划","有点慌然后冲","重排优先级","觉得计划无用"},{"Reschedule now","Panic then rush","Reprioritize","Plans useless"},3,{0,1,0,1} },
  { "更讨厌？","Annoy you more?",{"计划漏洞百出","计划束缚创意","频繁改期","完全没计划"},{"Holey plans","Stifling plans","Constant changes","No plan"},3,{0,1,0,1} },
  { "完美的一天结束？","Perfect day ends with?",{"清单清完心安","开心就好不管清单","完成关键一件","明天计划写好"},{"List cleared","Just happy","One key done","Plan tomorrow"},3,{0,1,0,1} },
},

/* ================= Bank 3 ================= */
{
  { "网络会议你常？","In online meetings you?",{"开麦多聊","基本静音","开麦但简短","用聊天区打字"},{"Speak a lot","Mostly mute","Short speak","Type in chat"},0,{0,1,0,1} },
  { "兴趣小组你？","Hobby group you?",{"很快成核心","安静跟练","张罗活动","按需参加"},{"Become core","Quiet follow","Organize","Join when needed"},0,{0,1,0,1} },
  { "复杂问题你倾向？","Complex problems you?",{"对话中厘清","闭关研究","找专家聊","资料+笔记"},{"Clarify in talk","Retreat & study","Ask experts","Notes & docs"},0,{0,1,0,1} },
  { "接推销电话你？","Sales calls you?",{"聊两句再说","尽快挂断","听清再说","直接挂"},{"Chat a bit","Hang up soon","Listen then","Hang up"},0,{0,1,0,1} },
  { "感到疲惫多因？","You feel drained by?",{"人太多话太多","独处太久无聊","无效社交","无法深入交流"},{"Too many people","Too much alone","Empty social","No deep talk"},0,{0,1,0,1} },
  { "小组作业你？","Group projects you?",{"对外沟通担当","后台资料担当","主持讨论","独立模块"},{"External talk","Backend docs","Facilitate","Solo module"},0,{0,1,0,1} },
  { "下班路上你？","Commute home you?",{"约人吃饭","希望没人找","听播客随意想","完全放空"},{"Dinner with people","Hope no one calls","Podcast wander","Zone out"},0,{0,1,0,1} },
  { "好消息你先？","Good news you first?",{"马上分享","先自己开心","发朋友圈","择人再说"},{"Share now","Enjoy first","Post online","Choose who"},0,{0,1,0,1} },
  { "认识的人派对？","Party of acquaintances?",{"很快玩成一片","想找角落","主动破冰","等别人来"},{"Warm fast","Find corner","Break ice","Wait others"},0,{0,1,0,1} },
  { "团队胜利你？","Team win you?",{"欢呼庆祝一起嗨","默默高兴","组织庆功","心里记着"},{"Cheer together","Quiet joy","Organize party","Keep inside"},0,{0,1,0,1} },
  { "发言前你需要？","Before speaking you need?",{"马上能说","完整想好","半想半说","写提纲"},{"Speak now","Fully formed","Half think","Outline"},0,{0,1,0,1} },
  { "交新邻居你？","New neighbors you?",{"主动串门","礼貌保持距离","交换联系方式","等对方先"},{"Visit first","Polite distance","Exchange contacts","Wait"},0,{0,1,0,1} },
  { "电量恢复来自？","Battery refills from?",{"热闹聚会后的余韵","安静房间","浅层闲聊","深度独处"},{"Afterparty glow","Quiet room","Light chat","Deep solitude"},0,{0,1,0,1} },
  { "真理更接近？","Truth is closer to?",{"可重复验证的事实","尚未证明的洞见","测量数据","内在直觉"},{"Repeatable facts","Unproven insight","Measured data","Inner sense"},1,{0,1,0,1} },
  { "PPT 你更爱？","Slides you prefer?",{"数据图表清晰","概念叙事线","清单要点","隐喻视觉"},{"Clear charts","Concept story","Bullet points","Metaphor visuals"},1,{0,1,0,1} },
  { "学软件你靠？","Learn software via?",{"教程步骤","理解原理后试","官方文档","视频演示"},{"Tutorial steps","Principle then try","Official docs","Video demo"},1,{0,1,0,1} },
  { "你常被说？","People say you are?",{"务实细心","有想象力","靠谱落地","想法很多"},{"Practical","Imaginative","Reliable","Full of ideas"},1,{0,1,0,1} },
  { "选餐厅你看？","Restaurant you pick?",{"评分与距离","装修氛围感","招牌菜数据","朋友说的意境"},{"Rating/distance","Ambience","Signature dish","Friends' vibe"},1,{0,1,0,1} },
  { "写周报你偏？","Weekly report you?",{"完成了ABC","思路与反思","数据表格","方向观察"},{"Did ABC","Thinking & lessons","Data tables","Direction watch"},1,{0,1,0,1} },
  { "空闲脑中更常？","Mind often wanders to?",{"待办具体步骤","如果…会怎样","清单顺序","人生可能性"},{"Task steps","What if","List order","Life maybes"},1,{0,1,0,1} },
  { "做菜失败你？","Dish fails you?",{"检查火候步骤","是不是创意不对","称量问题","灵感不够"},{"Check steps","Idea wrong?","Measure issue","No inspiration"},1,{0,1,0,1} },
  { "看建筑你注意？","Architecture you notice?",{"材料与结构","空间的情绪","尺寸数据","象征造型"},{"Material/structure","Spatial mood","Dimensions","Symbolic form"},1,{0,1,0,1} },
  { "团队招人你看？","Hiring you look at?",{"项目经历细节","潜力与灵气","可量化成绩","文化想象力"},{"Project details","Potential spark","Measurable wins","Cultural imagination"},1,{0,1,0,1} },
  { "地图导航你？","Navigation you?",{"看路名与距离","凭方向感","按导航走","想象城市结构"},{"Names & distance","Sense of direction","Follow nav","Imagine structure"},1,{0,1,0,1} },
  { "读小说偏好？","Novels you prefer?",{"写实细节饱满","寓意丰富","情节清晰","意识流"},{"Rich realism","Rich allegory","Clear plot","Stream of thought"},1,{0,1,0,1} },
  { "工作台面更像？","Your desk resembles?",{"工具分类摆放","灵感拼贴","整齐空旷","边做边摊"},{"Sorted tools","Inspiration collage","Clear empty","Messy while work"},1,{0,1,0,1} },
  { "决策依据你更认？","Decision basis you accept?",{"推理链完整","有没有伤人","证据质量","共识感受"},{"Full reasoning","Hurt no one","Evidence quality","Shared feelings"},2,{0,1,0,1} },
  { "朋友创业失败？","Friend's startup failed?",{"复盘商业逻辑","先陪他喝酒","看财务数据","心疼他的付出"},{"Business logic","Drink with him","Financials","Feel for effort"},2,{0,1,0,1} },
  { "资源分配你？","Resource allocation by?",{"投入产出比","谁更需要","KPI贡献","团队士气"},{"ROI","Who needs most","KPI","Morale"},2,{0,1,0,1} },
  { "提加薪你依据？","Raise case you base on?",{"职责与产出逻辑","自己很努力","市场与绩效数据","团队认可"},{"Role/output logic","Worked hard","Market/perf data","Team recognition"},2,{0,1,0,1} },
  { "好产品更是？","A good product is more?",{"逻辑自洽好用","让人感动","指标达标","有温度"},{"Coherent & useful","Moving","Metrics met","Warm"},2,{0,1,0,1} },
  { "规则该改时？","When rules should change?",{"逻辑上过时","总有人受伤","数据证明无效","大家都不满"},{"Logically outdated","People hurt","Data shows fail","Everyone unhappy"},2,{0,1,0,1} },
  { "选导师你看？","Choosing a mentor?",{"学术逻辑严谨","是否关心人","成果数据","口碑温度"},{"Rigour","Cares about people","Output data","Warm reputation"},2,{0,1,0,1} },
  { "服务差评你处理？","Bad reviews you handle?",{"核查事实流程","先共情回应","分析数据原因","安抚客户情绪"},{"Check facts","Empathize first","Data root cause","Sooth customer"},2,{0,1,0,1} },
  { "你更敬佩？","You admire more?",{"头脑清晰公正","温暖厚道","结果说话","让人安心"},{"Clear & fair","Warm & kind","Results speak","Make safe"},2,{0,1,0,1} },
  { "朋友说错话？","Friend misspeaks?",{"指出逻辑错误","看他是否难过","澄清事实","维护关系"},{"Point logic","Check if hurt","Clarify facts","Keep bond"},2,{0,1,0,1} },
  { "失败归因你看？","Failure you attribute to?",{"策略逻辑问题","支持不够","数据误判","氛围问题"},{"Strategy logic","Not enough support","Data misread","Atmosphere"},2,{0,1,0,1} },
  { "该不该说谎？","Is lying ok?",{"原则上不行","若保护人可以","看证据后果","看对方感受"},{"In principle no","If protects someone","Evidence/consequences","Their feelings"},2,{0,1,0,1} },
  { "高效会议更是？","Good meetings are more?",{"结论逻辑清晰","人人被听见","数据驱动","气氛不伤人"},{"Clear conclusions","All heard","Data-driven","Safe vibe"},2,{0,1,0,1} },
  { "行李打包你？","Packing you?",{"清单勾完","凭感觉塞","按天配装","轻装再说"},{"Checklist done","Stuff by feel","Outfits by day","Light later"},3,{0,1,0,1} },
  { "更新系统你？","OS updates you?",{"安排维护窗口","弹窗再说","备份后更新","等大版本"},{"Schedule window","When popup","Backup then","Wait major"},3,{0,1,0,1} },
  { "写论文你？","Thesis writing you?",{"提纲+日更","灵感来了猛写","按章节死线","最后冲刺"},{"Outline + daily","Write when inspired","Chapter deadlines","Final sprint"},3,{0,1,0,1} },
  { "健身计划你？","Workout plan?",{"课表固定","想去就去","按周期课表","跟朋友约"},{"Fixed plan","Whenever","Periodized","With friends"},3,{0,1,0,1} },
  { "买菜做饭你？","Groceries & cooking?",{"一周菜单采购","看冰箱剩啥","按食谱清单","叫外卖补充"},{"Weekly menu","Fridge leftovers","Recipe list","Order delivery"},3,{0,1,0,1} },
  { "桌面图标你？","Desktop icons you?",{"分组有序","密密麻麻","很少几个","按项目文件夹"},{"Grouped","Dense clutter","Very few","By project"},3,{0,1,0,1} },
  { "旅行照片你？","Travel photos you?",{"按日整理","拍完就丢相册","精选集","随手发"},{"By day","Dump album","Curated set","Post as go"},3,{0,1,0,1} },
  { "突发任务你？","Sudden task you?",{"插进日程重排","先做这个","评估优先级","心情好就做"},{"Insert & replan","Do it now","Assess priority","If mood"},3,{0,1,0,1} },
  { "读书笔记你？","Reading notes you?",{"结构化摘录","很少记","概念卡","有感就划"},{"Structured notes","Rarely","Concept cards","Mark when feel"},3,{0,1,0,1} },
  { "开放日程感觉？","Open schedule feels?",{"不安","自由","低效","舒服"},{"Uneasy","Free","Inefficient","Comfortable"},3,{0,1,0,1} },
  { "大扫除你？","Deep cleaning you?",{"安排固定日","实在看不下去","分区轮流","找人一起"},{"Scheduled day","When unbearable","Zone rotation","With help"},3,{0,1,0,1} },
  { "更讨厌哪种同事？","Colleague you dislike?",{"无视计划乱改","死守计划不变","总是最后一刻","从不给时间表"},{"Ignores plans","Never bends","Always last-minute","No timeline"},3,{0,1,0,1} },
  { "睡前你更想？","Before sleep you?",{"明日清单划掉","今天真开心","关键事已完","无所谓"},{"Tomorrow list cleared","Today was happy","Key done","Whatever"},3,{0,1,0,1} },
},

};

/* Fix bank2 item that had malformed strings - runtime uses only zh_opt/en_opt/pole */

static const char *const s_type_tip[16] = {
  "可靠务实，按规则把事做完。",
  "细致体贴，默默照顾身边人。",
  "有洞察，关心意义与他人成长。",
  "独立有远见，喜欢把系统想清楚。",
  "冷静动手强，现场解决问题。",
  "温和灵活，在意真实感受。",
  "理想主义，忠于内心价值。",
  "好奇分析型，爱拆解概念。",
  "反应快，喜欢直接行动。",
  "热情开朗，把当下过精彩。",
  "有感染力，爱探索新可能。",
  "机智善辩，喜欢挑战旧假设。",
  "组织力强，推动事情落地。",
  "热心合群，重视责任与关系。",
  "善引导他人，关注共同目标。",
  "果断有魄力，擅长定方向执行。",
};

static int s_bank;
static int s_idx;
static int s_sel[MBTI_N];
static int s_score[4][2];
static char s_type[5];
static int s_pct[4][2];

static lv_obj_t *s_prog;
static lv_obj_t *s_qtext;
static lv_obj_t *s_opt[4];
static lv_obj_t *s_bank_l;
static lv_obj_t *s_type_l;
static lv_obj_t *s_dim_l;
static lv_obj_t *s_tip_l;

static const mbti_item_t *cur_q(int i)
{
  if (s_bank < 0 || s_bank >= MBTI_BANK_N || i < 0 || i >= MBTI_N)
    {
      return &s_banks[0][0];
    }
  return &s_banks[s_bank][i];
}

static int type_index(const char *t)
{
  static const char *const codes[16] = {
    "ISTJ","ISFJ","INFJ","INTJ","ISTP","ISFP","INFP","INTP",
    "ESTP","ESFP","ENFP","ENTP","ESTJ","ESFJ","ENFJ","ENTJ"
  };
  int i;
  for (i = 0; i < 16; i++)
    {
      if (strcmp(t, codes[i]) == 0)
        {
          return i;
        }
    }
  return 0;
}

static void mbti_back_features(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_FEATURES);
}

static lv_obj_t *mk_page(dm_page_t id, const char *zh, const char *en)
{
  lv_obj_t *page = lv_obj_create(g_dm_root);
  lv_obj_t *back;
  lv_obj_t *title;

  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[id] = page;

  back = dm_btn(page, "←", "<", 36, 24, C_BTN, C_MUTED,
                mbti_back_features, NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(page, zh, en, g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);
  return page;
}

static void set_btn_text(lv_obj_t *btn, const char *zh, const char *en)
{
  lv_obj_t *lab;
  if (!btn)
    {
      return;
    }
  lab = lv_obj_get_child(btn, 0);
  if (lab)
    {
      lv_label_set_text(lab, dm_t(zh, en));
    }
}

static void compute_result(void)
{
  static const char first[4] = { 'E', 'S', 'T', 'J' };
  static const char second[4] = { 'I', 'N', 'F', 'P' };
  int i;
  int d;

  memset(s_score, 0, sizeof(s_score));
  for (i = 0; i < MBTI_N; i++)
    {
      int opt = s_sel[i];
      const mbti_item_t *q;
      if (opt < 0 || opt > 3)
        {
          continue;
        }
      q = cur_q(i);
      s_score[q->dim][q->pole[opt]]++;
    }
  for (d = 0; d < 4; d++)
    {
      int a = s_score[d][0];
      int b = s_score[d][1];
      int sum = a + b;
      if (sum <= 0)
        {
          sum = 1;
        }
      s_type[d] = (a >= b) ? first[d] : second[d];
      s_pct[d][0] = (a * 100) / sum;
      s_pct[d][1] = (b * 100) / sum;
    }
  s_type[4] = '\0';
}

static void paint_question(void)
{
  const mbti_item_t *q = cur_q(s_idx);
  char prog[24];
  int k;

  snprintf(prog, sizeof(prog), "%d/%d 套%d", s_idx + 1, MBTI_N, s_bank + 1);
  if (s_prog)
    {
      lv_label_set_text(s_prog, prog);
    }
  if (s_qtext)
    {
      lv_label_set_text(s_qtext, dm_t(q->zh, q->en));
    }
  for (k = 0; k < 4; k++)
    {
      set_btn_text(s_opt[k], q->zh_opt[k], q->en_opt[k]);
    }
}

static void paint_result(void)
{
  char line[80];
  int tip;

  compute_result();
  if (s_bank_l)
    {
      char b[24];
      snprintf(b, sizeof(b), dm_t("题库 %d / %d", "Bank %d / %d"),
               s_bank + 1, MBTI_BANK_N);
      lv_label_set_text(s_bank_l, b);
    }
  if (s_type_l)
    {
      lv_label_set_text(s_type_l, s_type);
    }
  if (s_dim_l)
    {
      snprintf(line, sizeof(line),
               "E %d%%  I %d%%\nS %d%%  N %d%%\nT %d%%  F %d%%\n"
               "J %d%%  P %d%%",
               s_pct[0][0], s_pct[0][1], s_pct[1][0], s_pct[1][1],
               s_pct[2][0], s_pct[2][1], s_pct[3][0], s_pct[3][1]);
      lv_label_set_text(s_dim_l, line);
    }
  tip = type_index(s_type);
  if (s_tip_l)
    {
      lv_label_set_text(s_tip_l, s_type_tip[tip]);
    }
}

static void pick_opt(int opt)
{
  if (s_idx < 0 || s_idx >= MBTI_N || opt < 0 || opt > 3)
    {
      return;
    }
  s_sel[s_idx] = opt;
  s_idx++;
  if (s_idx >= MBTI_N)
    {
      paint_result();
      dm_show(PAGE_MBTI_RESULT);
      return;
    }
  paint_question();
  dm_show(PAGE_MBTI_Q);
}

static void opt_cb(lv_event_t *e)
{
  pick_opt((int)(intptr_t)lv_event_get_user_data(e));
}

static void start_cb(lv_event_t *e)
{
  (void)e;
  s_bank = rand() % MBTI_BANK_N;
  s_idx = 0;
  memset(s_sel, 0xff, sizeof(s_sel));
  paint_question();
  dm_show(PAGE_MBTI_Q);
}

static void retry_cb(lv_event_t *e)
{
  start_cb(e); /* new random bank */
}

void dm_create_mbti(void)
{
  lv_obj_t *page;
  lv_obj_t *lab;
  lv_obj_t *b;
  int k;

  page = mk_page(PAGE_MBTI, "MBTI", "MBTI");
  lab = dm_lbl(page, "MBTI 性格测试", "MBTI personality", g_dm_font_m,
               C_INK);
  lv_obj_align(lab, LV_ALIGN_TOP_MID, 0, 44);
  lab = dm_lbl(page,
               "4 套题库 × 每套 52 题\n四选一（2×2）· 每次随机一套\n"
               "E/I · S/N · T/F · J/P 每维 13 题",
               "4 banks x 52 items\nRandom bank each test",
               g_dm_font_s, C_DIM);
  lv_label_set_long_mode(lab, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lab, 280);
  lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(lab, LV_ALIGN_TOP_MID, 0, 70);
  b = dm_btn(page, "开始测试", "Start", 160, 36, C_STAR, C_EYE, start_cb,
             NULL);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 150);

  page = mk_page(PAGE_MBTI_Q, "MBTI", "MBTI");
  s_prog = dm_lbl(page, "1/52 套1", "1/52 B1", g_dm_font_s, C_ACCENT);
  lv_obj_set_pos(s_prog, 200, 12);
  s_qtext = dm_lbl(page, "", "", g_dm_font_s, C_INK);
  lv_label_set_long_mode(s_qtext, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_qtext, 288);
  lv_obj_set_pos(s_qtext, 16, 42);

  for (k = 0; k < 4; k++)
    {
      int col = k % 2;
      int row = k / 2;
      s_opt[k] = dm_btn(page, "…", "...", 146, 42, C_BTN, C_INK, opt_cb,
                        (void *)(intptr_t)k);
      lv_obj_set_pos(s_opt[k], 12 + col * 150, 118 + row * 48);
    }
  lab = dm_lbl(page, "选最像你的一项", "Pick what fits most",
               g_dm_font_s, C_MUTED);
  lv_obj_align(lab, LV_ALIGN_TOP_MID, 0, 214);

  page = mk_page(PAGE_MBTI_RESULT, "MBTI 结果", "MBTI result");
  s_bank_l = dm_lbl(page, "题库 1 / 4", "Bank 1 / 4", g_dm_font_s, C_DIM);
  lv_obj_align(s_bank_l, LV_ALIGN_TOP_MID, 0, 36);
  s_type_l = dm_lbl(page, "----", "----", g_dm_font_xl, C_STAR);
  lv_obj_align(s_type_l, LV_ALIGN_TOP_MID, 0, 52);
  s_dim_l = dm_lbl(page, "", "", g_dm_font_s, C_INK);
  lv_label_set_long_mode(s_dim_l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_dim_l, 280);
  lv_obj_set_style_text_align(s_dim_l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_dim_l, LV_ALIGN_TOP_MID, 0, 96);
  s_tip_l = dm_lbl(page, "", "", g_dm_font_s, C_DIM);
  lv_label_set_long_mode(s_tip_l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_tip_l, 260);
  lv_obj_set_style_text_align(s_tip_l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_tip_l, LV_ALIGN_TOP_MID, 0, 172);
  b = dm_btn(page, "再测一次", "Retest", 100, 30, C_STAR, C_EYE, retry_cb,
             NULL);
  lv_obj_set_pos(b, 50, 200);
  b = dm_btn(page, "返回", "Back", 100, 30, C_BTN, C_MUTED,
             mbti_back_features, NULL);
  lv_obj_set_pos(b, 170, 200);

  s_bank = 0;
  s_idx = 0;
  memset(s_sel, 0xff, sizeof(s_sel));
}

#endif /* CONFIG_DESKMATE_APP */
