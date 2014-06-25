#include "widget.h"
#include "ui_widget.h"
#include "settings.h"
#include "friend.h"
#include "friendlist.h"
#include "friendrequestdialog.h"
#include "friendwidget.h"
#include "grouplist.h"
#include "group.h"
#include "groupwidget.h"
#include "groupchatform.h"
#include <QMessageBox>
#include <QDebug>

Widget *Widget::instance{nullptr};

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
    ui->mainContent->setLayout(new QVBoxLayout());
    ui->mainHead->setLayout(new QVBoxLayout());
    ui->mainHead->layout()->setMargin(0);
    ui->mainHead->layout()->setSpacing(0);
    QWidget* friendListWidget = new QWidget();
    friendListWidget->setLayout(new QVBoxLayout());
    friendListWidget->layout()->setSpacing(0);
    friendListWidget->layout()->setMargin(0);
    friendListWidget->setLayoutDirection(Qt::LeftToRight);
    ui->friendList->setWidget(friendListWidget);

    ui->nameLabel->setText(Settings::getInstance().getUsername());
    ui->statusLabel->setText(Settings::getInstance().getStatusMessage());
    ui->friendList->widget()->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    qRegisterMetaType<Status>("Status");
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<int32_t>("int32_t");

    core = new Core();
    coreThread = new QThread(this);
    core->moveToThread(coreThread);
    connect(coreThread, &QThread::started, core, &Core::start);

    connect(core, &Core::connected, this, &Widget::onConnected);
    connect(core, &Core::disconnected, this, &Widget::onDisconnected);
    connect(core, &Core::failedToStart, this, &Widget::onFailedToStartCore);
    connect(core, &Core::statusSet, this, &Widget::onStatusSet);
    connect(core, &Core::usernameSet, this, &Widget::setUsername);
    connect(core, &Core::statusMessageSet, this, &Widget::setStatusMessage);
    connect(core, &Core::friendAddressGenerated, &settingsForm, &SettingsForm::setFriendAddress);
    connect(core, &Core::friendAdded, this, &Widget::addFriend);
    connect(core, &Core::friendStatusChanged, this, &Widget::onFriendStatusChanged);
    connect(core, &Core::friendUsernameChanged, this, &Widget::onFriendUsernameChanged);
    connect(core, &Core::friendStatusChanged, this, &Widget::onFriendStatusChanged);
    connect(core, &Core::friendStatusMessageChanged, this, &Widget::onFriendStatusMessageChanged);
    connect(core, &Core::friendUsernameLoaded, this, &Widget::onFriendUsernameLoaded);
    connect(core, &Core::friendStatusMessageLoaded, this, &Widget::onFriendStatusMessageLoaded);
    connect(core, &Core::friendRequestReceived, this, &Widget::onFriendRequestReceived);
    connect(core, &Core::friendMessageReceived, this, &Widget::onFriendMessageReceived);
    connect(core, &Core::groupInviteReceived, this, &Widget::onGroupInviteReceived);
    connect(core, &Core::groupMessageReceived, this, &Widget::onGroupMessageReceived);
    connect(core, &Core::groupNamelistChanged, this, &Widget::onGroupNamelistChanged);

    connect(this, &Widget::statusSet, core, &Core::setStatus);
    connect(this, &Widget::friendRequested, core, &Core::requestFriendship);
    connect(this, &Widget::friendRequestAccepted, core, &Core::acceptFriendRequest);

    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(onAddClicked()));
    connect(ui->groupButton, SIGNAL(clicked()), this, SLOT(onGroupClicked()));
    connect(ui->transferButton, SIGNAL(clicked()), this, SLOT(onTransferClicked()));
    connect(ui->settingsButton, SIGNAL(clicked()), this, SLOT(onSettingsClicked()));
    connect(ui->nameLabel, SIGNAL(textChanged(QString,QString)), this, SLOT(onUsernameChanged(QString,QString)));
    connect(ui->statusLabel, SIGNAL(textChanged(QString,QString)), this, SLOT(onStatusMessageChanged(QString,QString)));
    connect(&settingsForm.name, SIGNAL(textChanged(QString)), this, SLOT(onUsernameChanged(QString)));
    connect(&settingsForm.statusText, SIGNAL(textChanged(QString)), this, SLOT(onStatusMessageChanged(QString)));
    connect(&friendForm, SIGNAL(friendRequested(QString,QString)), this, SIGNAL(friendRequested(QString,QString)));

    coreThread->start();

    friendForm.show(*ui);

    // TODO: For the friendlist just stack friend widgets in a scrollable widget's layout
}

Widget::~Widget()
{
    instance = nullptr;
    coreThread->exit();
    coreThread->wait();
    delete core;

    hideMainForms();

    for (Friend* f : FriendList::friendList)
        delete f;
    FriendList::friendList.clear();
    for (Group* g : GroupList::groupList)
        delete g;
    GroupList::groupList.clear();

    delete ui;
}

Widget* Widget::getInstance()
{
    if (!instance)
        instance = new Widget();
    return instance;
}

QString Widget::getUsername()
{
    return ui->nameLabel->text();
}

void Widget::onConnected()
{
    emit statusSet(Status::Online);
}

void Widget::onDisconnected()
{
    emit statusSet(Status::Offline);
}

void Widget::onFailedToStartCore()
{
    QMessageBox critical(this);
    critical.setText("Toxcor failed to start, the application will terminate after you close this message.");
    critical.setIcon(QMessageBox::Critical);
    critical.exec();
    qApp->quit();
}

void Widget::onStatusSet(Status status)
{
    if (status == Status::Online)
        ui->statImg->setPixmap(QPixmap("img/status/dot_online_2x.png"));
    else if (status == Status::Busy || status == Status::Away)
        ui->statImg->setPixmap(QPixmap("img/status/dot_idle_2x.png"));
    else if (status == Status::Offline)
        ui->statImg->setPixmap(QPixmap("img/status/dot_away_2x.png"));
}

void Widget::onAddClicked()
{
    hideMainForms();
    friendForm.show(*ui);
}

void Widget::onGroupClicked()
{

}

void Widget::onTransferClicked()
{

}

void Widget::onSettingsClicked()
{
    hideMainForms();
    settingsForm.show(*ui);
}

void Widget::hideMainForms()
{
    QLayoutItem* item;
    while ((item = ui->mainHead->layout()->takeAt(0)) != 0)
        item->widget()->hide();
    while ((item = ui->mainContent->layout()->takeAt(0)) != 0)
        item->widget()->hide();
}

void Widget::onUsernameChanged(const QString& newUsername)
{
    ui->nameLabel->setText(newUsername);
    settingsForm.name.setText(newUsername);
    core->setUsername(newUsername);
}

void Widget::onUsernameChanged(const QString& newUsername, const QString& oldUsername)
{
    ui->nameLabel->setText(oldUsername); // restore old username until Core tells us to set it
    settingsForm.name.setText(oldUsername);
    core->setUsername(newUsername);
}

void Widget::setUsername(const QString& username)
{
    ui->nameLabel->setText(username);
    settingsForm.name.setText(username);
    Settings::getInstance().setUsername(username);
}

void Widget::onStatusMessageChanged(const QString& newStatusMessage)
{
    ui->statusLabel->setText(newStatusMessage);
    settingsForm.statusText.setText(newStatusMessage);
    core->setStatusMessage(newStatusMessage);
}

void Widget::onStatusMessageChanged(const QString& newStatusMessage, const QString& oldStatusMessage)
{
    ui->statusLabel->setText(oldStatusMessage); // restore old status message until Core tells us to set it
    settingsForm.statusText.setText(oldStatusMessage);
    core->setStatusMessage(newStatusMessage);
}

void Widget::setStatusMessage(const QString &statusMessage)
{
    ui->statusLabel->setText(statusMessage);
    settingsForm.statusText.setText(statusMessage);
    Settings::getInstance().setStatusMessage(statusMessage);
}

void Widget::addFriend(int friendId, const QString &userId)
{
    Friend* newfriend = FriendList::addFriend(friendId, userId);
    QWidget* widget = ui->friendList->widget();
    QLayout* layout = widget->layout();
    layout->addWidget(newfriend->widget);
    connect(newfriend->widget, SIGNAL(friendWidgetClicked(FriendWidget*)), this, SLOT(onFriendWidgetClicked(FriendWidget*)));
    connect(newfriend->widget, SIGNAL(removeFriend(int)), this, SLOT(removeFriend(int)));
    connect(newfriend->chatForm, SIGNAL(sendMessage(int,QString)), core, SLOT(sendMessage(int,QString)));
    connect(newfriend->chatForm, SIGNAL(sendFile(int32_t,QString,QByteArray)), core, SLOT(sendFile(int32_t,QString,QByteArray)));
}

void Widget::onFriendStatusChanged(int friendId, Status status)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    if (status == Status::Online)
        f->widget->statusPic.setPixmap(QPixmap("img/status/dot_online.png"));
    else if (status == Status::Busy || status == Status::Away)
        f->widget->statusPic.setPixmap(QPixmap("img/status/dot_idle.png"));
    else if (status == Status::Offline)
        f->widget->statusPic.setPixmap(QPixmap("img/status/dot_away.png"));
}

void Widget::onFriendStatusMessageChanged(int friendId, const QString& message)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->setStatusMessage(message);
}

void Widget::onFriendUsernameChanged(int friendId, const QString& username)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->setName(username);
}

void Widget::onFriendStatusMessageLoaded(int friendId, const QString& message)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->setStatusMessage(message);
}

void Widget::onFriendUsernameLoaded(int friendId, const QString& username)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->setName(username);
}

void Widget::onFriendWidgetClicked(FriendWidget *widget)
{
    Friend* f = FriendList::findFriend(widget->friendId);
    if (!f)
        return;

    hideMainForms();
    f->chatForm->show(*ui);
}

void Widget::onFriendMessageReceived(int friendId, const QString& message)
{
    Friend* f = FriendList::findFriend(friendId);
    if (!f)
        return;

    f->chatForm->addFriendMessage(message);
}

void Widget::onFriendRequestReceived(const QString& userId, const QString& message)
{
    FriendRequestDialog dialog(this, userId, message);

    if (dialog.exec() == QDialog::Accepted)
        emit friendRequestAccepted(userId);
}

void Widget::removeFriend(int friendId)
{
    Friend* f = FriendList::findFriend(friendId);
    FriendList::removeFriend(friendId);
    core->removeFriend(friendId);
    delete f;
    if (ui->mainHead->layout()->isEmpty())
        onAddClicked();
}

void Widget::onGroupInviteReceived(int friendId, uint8_t* publicKey)
{
    int groupId = core->joinGroupchat(friendId, publicKey);
    if (groupId == -1)
    {
        qWarning() << "Widget::onGroupInviteReceived: Unable to accept invitation";
        return;
    }
    //createGroup(groupId);
}

void Widget::onGroupMessageReceived(int groupnumber, int friendgroupnumber, const QString& message)
{
    Group* g = GroupList::findGroup(groupnumber);
    if (!g)
        return;

    g->chatForm->addGroupMessage(message, friendgroupnumber);
}

void Widget::onGroupNamelistChanged(int groupnumber, int peernumber, uint8_t Change)
{
    Group* g = GroupList::findGroup(groupnumber);
    if (!g)
    {
        qDebug() << "Widget::onGroupNamelistChanged: Group not found, creating it";
        g = createGroup(groupnumber);
    }

    TOX_CHAT_CHANGE change = static_cast<TOX_CHAT_CHANGE>(Change);
    if (change == TOX_CHAT_CHANGE_PEER_ADD)
        g->addPeer(peernumber,"<Unknown>");
    else if (change == TOX_CHAT_CHANGE_PEER_DEL)
        g->removePeer(peernumber);
    else if (change == TOX_CHAT_CHANGE_PEER_NAME)
        g->updatePeer(peernumber,core->getGroupPeerName(groupnumber, peernumber));
}

void Widget::onGroupWidgetClicked(GroupWidget* widget)
{
    Group* g = GroupList::findGroup(widget->groupId);
    if (!g)
        return;

    hideMainForms();
    g->chatForm->show(*ui);
}

void Widget::removeGroup(int groupId)
{
    Group* g = GroupList::findGroup(groupId);
    GroupList::removeGroup(groupId);
    core->removeGroup(groupId);
    delete g;
    if (ui->mainHead->layout()->isEmpty())
        onAddClicked();
}

const Core* Widget::getCore()
{
    return core;
}

Group *Widget::createGroup(int groupId)
{
    Group* g = GroupList::findGroup(groupId);
    if (g)
    {
        qWarning() << "Widget::createGroup: Group already exists";
        return g;
    }

    QString groupName = QString("Groupchat #%1").arg(groupId);
    Group* newgroup = GroupList::addGroup(groupId, groupName);
    QWidget* widget = ui->friendList->widget();
    QLayout* layout = widget->layout();
    layout->addWidget(newgroup->widget);
    connect(newgroup->widget, SIGNAL(groupWidgetClicked(GroupWidget*)), this, SLOT(onGroupWidgetClicked(GroupWidget*)));
    connect(newgroup->widget, SIGNAL(removeGroup(int)), this, SLOT(removeGroup(int)));
    connect(newgroup->chatForm, SIGNAL(sendMessage(int,QString)), core, SLOT(sendGroupMessage(int,QString)));
    return newgroup;
}