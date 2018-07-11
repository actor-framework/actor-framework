#!/usr/bin/env groovy

// Our build matrix. The keys are the operating system labels and the values
// are lists of tool labels.
buildMatrix = [
  // Debug builds with ASAN + logging for various OS/compiler combinations.
  ['unix', [
    cmakeArgs: '-D CAF_LOG_LEVEL=4 '
               + '-D CAF_ENABLE_ADDRESS_SANITIZER:BOOL=yes',
    builds: ['debug'],
    tools: ['gcc4.8', 'gcc4.9', 'gcc5', 'gcc6', 'gcc7', 'gcc8', 'clang'],
  ]],
  // One release build per supported OS. FreeBSD and Windows have the least
  // testing outside Jenkins, so we also explicitly schedule debug builds.
  ['Linux', [
    builds: ['release'],
    tools: ['gcc', 'clang'],
  ]],
  ['macOS', [
    builds: ['release'],
    tools: ['clang'],
  ]],
  ['FreeBSD', [
    builds: ['debug'], // no release build for now, because it takes 1h
    tools: ['clang'],
  ]],
  ['Windows', [
    builds: ['debug', 'release'],
    tools: ['msvc'],
  ]],
  // One Additional build for coverage reports.
  ['unix', [
    cmakeArgs: '-D CAF_ENABLE_GCOV:BOOL=yes '
               + '-D CAF_NO_EXCEPTIONS:BOOL=yes '
               + '-D CAF_FORCE_NO_EXCEPTIONS:BOOL=yes',
    builds: ['debug'],
    tools: ['gcovr'],
    extraSteps: ['coverageReport'],
  ]],
]

// Optional environment variables for combinations of labels.
buildEnvironments = [
  'macOS && gcc': ['CXX=g++'],
  'Linux && clang': ['CXX=clang++'],
]

// Builds options on UNIX.
unixOpts = "-DCAF_NO_PROTOBUF_EXAMPLES:BOOL=yes " +
           "-DCAF_NO_QT_EXAMPLES:BOOL=yes " +
           "-DCAF_MORE_WARNINGS:BOOL=yes " +
           "-DCAF_ENABLE_RUNTIME_CHECKS:BOOL=yes " +
           "-DCAF_NO_BENCHMARKS:BOOL=yes " +
           "-DOPENSSL_ROOT_DIR=/usr/local/opt/openssl " +
           "-DOPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include"

// Builds options on Windows.
msOpts = "-DCAF_BUILD_STATIC_ONLY:BOOL=yes " +
         "-DCAF_NO_BENCHMARKS:BOOL=yes " +
         "-DCAF_NO_EXAMPLES:BOOL=yes " +
         "-DCAF_NO_MEM_MANAGEMENT:BOOL=yes " +
         "-DCAF_NO_OPENCL:BOOL=yes " +
         "-DCAF_LOG_LEVEL:INT=0 "

// Called *after* a build succeeded.
def coverageReport() {
  dir('caf-sources') {
    sh 'gcovr -e libcaf_test -e ".*/test/.*" -x -r .. > coverage.xml'
    archiveArtifacts '**/coverage.xml'
    cobertura([
      autoUpdateHealth: false,
      autoUpdateStability: false,
      coberturaReportFile: '**/coverage.xml',
      conditionalCoverageTargets: '70, 0, 0',
      failUnhealthy: false,
      failUnstable: false,
      lineCoverageTargets: '80, 0, 0',
      maxNumberOfBuilds: 0,
      methodCoverageTargets: '80, 0, 0',
      onlyStable: false,
      sourceEncoding: 'ASCII',
      zoomCoverageChart: false,
    ])
  }
}

def buildSteps(buildType, cmakeArgs) {
  echo "build stage: $STAGE_NAME"
  deleteDir()
  unstash('caf-sources')
  dir('caf-sources') {
    if (STAGE_NAME.contains('Windows')) {
      echo "Windows build on $NODE_NAME"
      withEnv(['PATH=C:\\Windows\\System32;C:\\Program Files\\CMake\\bin;C:\\Program Files\\Git\\cmd;C:\\Program Files\\Git\\bin']) {
          // Configure and build.
          cmakeBuild([
            buildDir: 'build',
            buildType: "$buildType",
            cmakeArgs: "$msOpts $cmakeArgs",
            generator: 'Visual Studio 15 2017',
            installation: 'cmake in search path',
            sourceDir: '.',
            steps: [[withCmake: true]],
          ])
          // Test.
          ctest([
            arguments: '--output-on-failure',
            installation: 'cmake auto install',
            workingDir: 'build',
          ])
      }
    } else {
      echo "Unix build on $NODE_NAME"
      def leakCheck = STAGE_NAME.contains("Linux") && !STAGE_NAME.contains("clang")
      withEnv(["label_exp=" + STAGE_NAME.toLowerCase(),
               "ASAN_OPTIONS=detect_leaks=" + (leakCheck ? 1 : 0)]) {
        // Configure and build.
        cmakeBuild([
          buildDir: 'build',
          buildType: "$buildType",
          cmakeArgs: "$unixOpts $cmakeArgs",
          generator: 'Unix Makefiles',
          installation: 'cmake in search path',
          preloadScript: '../cmake/jenkins.cmake',
          sourceDir: '.',
          steps: [[args: 'all']],
        ])
        // Test.
        ctest([
          arguments: '--output-on-failure',
          installation: 'cmake in search path',
          workingDir: 'build',
        ])
      }
    }
  }
}

// Builds a stage for given builds. Results in a parallel stage `if builds.size() > 1`.
def makeBuildStages(matrixIndex, builds, lblExpr, settings) {
  builds.collectEntries { buildType ->
    def id = "$matrixIndex $lblExpr: $buildType"
    [
      (id):
      {
        node(lblExpr) {
          stage(id) {
            try {
              withEnv(buildEnvironments[lblExpr] ?: []) {
                buildSteps(buildType, settings['cmakeArgs'] ?: '')
                (settings['extraSteps'] ?: []).each { fun -> "$fun"() }
              }
            } finally {
              cleanWs()
            }
          }
        }
      }
    ]
  }
}

pipeline {
  agent none
  environment {
    LD_LIBRARY_PATH = "$WORKSPACE/caf-sources/build/lib"
    DYLD_LIBRARY_PATH = "$WORKSPACE/caf-sources/build/lib"
  }
  stages {
    stage ('Git Checkout') {
      agent { label 'master' }
      steps {
        deleteDir()
        dir('caf-sources') {
          checkout scm
        }
        stash includes: 'caf-sources/**', name: 'caf-sources'
      }
    }
    // Start builds.
    stage('Builds') {
      steps {
        script {
          // Create stages for building everything in our build matrix in
          // parallel.
          def xs = [:]
          buildMatrix.eachWithIndex { entry, index ->
              def (os, settings) = entry
              settings['tools'].eachWithIndex { tool, toolIndex ->
                def matrixIndex = "[$index:$toolIndex]"
                def builds = settings['builds']
                def labelExpr = "$os && $tool"
                xs << makeBuildStages(matrixIndex, builds, labelExpr, settings)
              }
          }
          parallel xs
        }
      }
    }
  }
  post {
    success {
      echo "Success."
      // TODO: Gitter?
    }
    failure {
      echo "Failure, sending mails!"
      // TODO: Gitter?
      emailext(
        subject: "FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]'",
        body: """FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]':
                 Check console output at "${env.BUILD_URL}"
                 Job: ${env.JOB_NAME}
                 Build number: ${env.BUILD_NUMBER}""",
        // TODO: Uncomment the following line:
        // recipientProviders: [[$class: 'CulpritsRecipientProvider'], [$class: 'RequesterRecipientProvider']]
        to: 'hiesgen;neverlord' // add multiple separated with ';'
      )
    }
  }
}

