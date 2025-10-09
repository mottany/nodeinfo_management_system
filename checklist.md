# チェックリスト

- [ ] ローカルネットワーク内の処理
  - [x] member_node_procedure から master_node_procedure に切り替えができる
  - [x] member node は master node にクラスタ参加要求できる
  - [x] master node は member node の要求を受信できる
  - [ ] master node は member node の要求に応答できる
  - [ ] member node は master node の応答を受信できる
  - [ ] member node は master node に自身の nodedata を送信できる
  - [ ] master node は member node から nodedata を受信できる
  - [ ] master node は nodedata_list に nodedata を追加できる
  - [ ] master node は member node に nodedata_list を送信できる
  - [ ] member node は nodeinfo と hostfile を作成/更新できる

- [ ] ネットワークを跨いだ処理
  - [ ] master node は relay server にクラスタ参加要求できる
  - [ ] relay server は master node のクラスタ参加要求に応答できる
  - [ ] relay server は nodeinfo_database を作成できる
  - [ ] relay server は master node に nodeinfo_database を送信できる
  - [ ] master node は relay server から nodeinfo_database を受信できる
  - [ ] master node は member node に nodeinfo_database を送信できる

- [ ] ノード脱退処理
  - [ ] member node は master node に脱退要求できる
  - [ ] master node は member node からの脱退要求を受信できる
  - [ ] master node は nodedata_list から nodedata を削除できる
