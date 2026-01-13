#pragma once

#include <QWizardPage>

#include <vector>

class Auth;
class Ui_AutoConfigStreamPage;

class AutoConfigStreamPage : public QWizardPage {
	Q_OBJECT

	friend class AutoConfig;

	enum class Section : int {
		Connect,
		StreamKey,
	};

	std::shared_ptr<Auth> auth;

	std::unique_ptr<Ui_AutoConfigStreamPage> ui;
	QString lastService;
	bool ready = false;
	struct StreamTargetEntry {
		QString platform;
		QString server;
		QString key;
	};
	std::vector<StreamTargetEntry> streamTargets;

	void LoadServices(bool showAll);
	inline bool IsCustomService() const;
	void InitializeMultiServiceTargetsUI();
	void AddStreamTargetRow(const StreamTargetEntry &entry);
	std::vector<StreamTargetEntry> CollectStreamTargets() const;
	void UpdateTargetButtons();

public:
	AutoConfigStreamPage(QWidget *parent = nullptr);
	~AutoConfigStreamPage();

	virtual bool isComplete() const override;
	virtual int nextId() const override;
	virtual bool validatePage() override;

	void OnAuthConnected();
	void OnOAuthStreamKeyConnected();

public slots:
	void on_show_clicked();
	void on_connectAccount_clicked();
	void on_disconnectAccount_clicked();
	void on_useStreamKey_clicked();
	void on_addTargetButton_clicked();
	void on_removeTargetButton_clicked();
	void on_preferHardware_clicked();
	void ServiceChanged();
	void UpdateKeyLink();
	void UpdateMoreInfoLink();
	void UpdateServerList();
	void UpdateCompleted();

	void reset_service_ui_fields(std::string &service);
};
